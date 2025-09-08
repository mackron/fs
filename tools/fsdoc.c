/*
API Documentation Generator

This tool extracts API documentation from comments and function declarations.

This program is entirely AI generated, and as such it is very rough and dirty. It is built specifically
for my own internal use for my own libraries and is not suitable for general production use.
*/

/* Disable MSVC security warnings for unsafe string functions */
#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include "../fs.c"
#include <stdio.h>

/* Manual character type checking to avoid ctype.h issues with MSVC */
#define FSDOC_IS_UPPER(c)  ((c) >= 'A' && (c) <= 'Z')
#define FSDOC_IS_DIGIT(c)  ((c) >= '0' && (c) <= '9')
#define FSDOC_IS_SPACE(c)  ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\f' || (c) == '\v')

/* Simple API function structure */
typedef struct fsdoc_see_also
{
    char name[128];
    struct fsdoc_see_also* pNext;
} fsdoc_see_also;

typedef struct fsdoc_example
{
    char title[256];
    char* pContent;
    struct fsdoc_example* pNext;
} fsdoc_example;

typedef struct fsdoc_param
{
    char type[256];
    char name[128];
    char direction[32];    /* "in", "out", "in/out" */
    char flags[64];        /* "optional", etc. */
    char* pDescription;    /* Parameter description from docs */
    int isDocumented;      /* 1 if found in docs, 0 if only in declaration */
    struct fsdoc_param* pNext;
} fsdoc_param;

typedef struct fsdoc_function
{
    char name[128];
    char return_type[256];
    char* pComment;
    char* pDescription;
    char* pReturnValue;
    fsdoc_param* pFirstParam;
    fsdoc_see_also* pFirstSeeAlso;
    fsdoc_example* pFirstExample;
    struct fsdoc_function* pNext;
} fsdoc_function;

typedef struct fsdoc_context
{
    fsdoc_function* pFirstFunction;
    fs_file* pInputFile;
    char* pFileContent;
    size_t fileSize;
    size_t cursor;
} fsdoc_context;

/* Forward declarations */
static int fsdoc_init(fsdoc_context* pContext, const char* pFilePath);
static void fsdoc_uninit(fsdoc_context* pContext);
static int fsdoc_parse(fsdoc_context* pContext);
static int fsdoc_output_json(fsdoc_context* pContext, const char* pOutputPath);
static int fsdoc_output_markdown(fsdoc_context* pContext, const char* pOutputPath);

/* Helper functions */
static void fsdoc_trim_whitespace(char* pStr);
static void fsdoc_normalize_indentation(char* pStr);
static char* fsdoc_convert_options_to_table(const char* pStr, const fs_allocation_callbacks* pAllocationCallbacks);
static char* fsdoc_extract_comment_before_function(fsdoc_context* pContext, size_t functionPos);
static int fsdoc_parse_function_declaration(const char* pDeclaration, fsdoc_function* pFunction);
static void fsdoc_parse_see_also(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_examples(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_return_value(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_description(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_parameters_docs(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_free_function(fsdoc_function* pFunction);
static void fsdoc_free_params(fsdoc_param* pParam);
static void fsdoc_free_see_also(fsdoc_see_also* pSeeAlso);
static void fsdoc_free_examples(fsdoc_example* pExample);
static void fsdoc_write_json_string(fs_file* pFile, const char* pStr);

int main(int argc, char** argv)
{
    fsdoc_context context;
    int result;

    if (argc != 2) {
        printf("Usage: fsdoc <header-file>\n");
        printf("Example: fsdoc fs.h\n");
        return 1;
    }

    /* Initialize the context */
    result = fsdoc_init(&context, argv[1]);
    if (result != 0) {
        printf("Failed to initialize context for file: %s\n", argv[1]);
        return result;
    }

    /* Parse the file */
    result = fsdoc_parse(&context);
    if (result != 0) {
        printf("Failed to parse file: %s\n", argv[1]);
        fsdoc_uninit(&context);
        return result;
    }

    /* Output documentation */
    printf("Generating API documentation...\n");
    
    /* Create docs directory if it doesn't exist */
    fs_mkdir(NULL, "docs", 0);
    
    result = fsdoc_output_json(&context, "docs/api.json");
    if (result != 0) {
        printf("Failed to generate JSON output\n");
    } else {
        printf("Generated docs/api.json\n");
    }

    result = fsdoc_output_markdown(&context, "docs/api.md");
    if (result != 0) {
        printf("Failed to generate Markdown output\n");
    } else {
        printf("Generated docs/api.md\n");
    }

    fsdoc_uninit(&context);
    return 0;
}

static int fsdoc_init(fsdoc_context* pContext, const char* pFilePath)
{
    fs_result result;
    fs_file_info fileInfo;

    if (pContext == NULL || pFilePath == NULL) {
        return 1;
    }

    memset(pContext, 0, sizeof(*pContext));

    /* Open the file using fs API */
    result = fs_file_open(NULL, pFilePath, FS_READ, &pContext->pInputFile);
    if (result != FS_SUCCESS) {
        return 1;
    }

    /* Get file size */
    result = fs_file_get_info(pContext->pInputFile, &fileInfo);
    if (result != FS_SUCCESS) {
        fs_file_close(pContext->pInputFile);
        return 1;
    }

    pContext->fileSize = (size_t)fileInfo.size;

    /* Read entire file into memory */
    pContext->pFileContent = fs_malloc(pContext->fileSize + 1, NULL);
    if (pContext->pFileContent == NULL) {
        fs_file_close(pContext->pInputFile);
        return 1;
    }

    result = fs_file_read(pContext->pInputFile, pContext->pFileContent, pContext->fileSize, NULL);
    if (result != FS_SUCCESS) {
        fs_free(pContext->pFileContent, NULL);
        fs_file_close(pContext->pInputFile);
        return 1;
    }

    pContext->pFileContent[pContext->fileSize] = '\0';
    
    return 0;
}

static void fsdoc_uninit(fsdoc_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    if (pContext->pInputFile != NULL) {
        fs_file_close(pContext->pInputFile);
    }

    if (pContext->pFileContent != NULL) {
        fs_free(pContext->pFileContent, NULL);
    }

    /* Free function list */
    while (pContext->pFirstFunction != NULL) {
        fsdoc_function* pNext = pContext->pFirstFunction->pNext;
        fsdoc_free_function(pContext->pFirstFunction);
        pContext->pFirstFunction = pNext;
    }

    memset(pContext, 0, sizeof(*pContext));
}

static int fsdoc_parse(fsdoc_context* pContext)
{
    char* pCurrent;
    char* pLine;
    char* pNextLine;
    fsdoc_function* pLastFunction;

    if (pContext == NULL || pContext->pFileContent == NULL) {
        return 1;
    }

    pLastFunction = NULL;
    pCurrent = pContext->pFileContent;

    while (*pCurrent != '\0') {
        /* Find the next line that starts with "FS_API" */
        pLine = strstr(pCurrent, "FS_API");
        if (pLine == NULL) {
            break;
        }

        /* Make sure it's at the beginning of a line */
        if (pLine != pContext->pFileContent && *(pLine - 1) != '\n') {
            pCurrent = pLine + 6;
            continue;
        }

        /* Find the end of this declaration (semicolon) */
        pNextLine = strchr(pLine, ';');
        if (pNextLine == NULL) {
            break;
        }
        pNextLine++;

        /* Extract the function declaration */
        size_t declarationLen = pNextLine - pLine;
        char* pDeclaration = fs_malloc(declarationLen + 1, NULL);
        if (pDeclaration == NULL) {
            return 1;
        }

        strncpy(pDeclaration, pLine, declarationLen);
        pDeclaration[declarationLen] = '\0';

        /* Create a new function */
        fsdoc_function* pFunction = fs_malloc(sizeof(fsdoc_function), NULL);
        if (pFunction == NULL) {
            fs_free(pDeclaration, NULL);
            return 1;
        }

        memset(pFunction, 0, sizeof(*pFunction));

        /* Parse the function declaration */
        if (fsdoc_parse_function_declaration(pDeclaration, pFunction) == 0) {
            /* Extract any comment before this function */
            char* pComment = fsdoc_extract_comment_before_function(pContext, pLine - pContext->pFileContent);
            if (pComment != NULL) {
                /* Allocate memory for the comment and copy it */
                size_t commentLen = strlen(pComment);
                pFunction->pComment = fs_malloc(commentLen + 1, NULL);
                if (pFunction->pComment != NULL) {
                    strcpy(pFunction->pComment, pComment);
                } else {
                    pFunction->pComment = NULL;
                }
                
                /* Parse main description from the comment */
                fsdoc_parse_description(pComment, pFunction);
                
                /* Parse parameter documentation from the comment */
                fsdoc_parse_parameters_docs(pComment, pFunction);
                
                /* Parse See Also section from the comment */
                fsdoc_parse_see_also(pComment, pFunction);
                
                /* Parse Examples from the comment */
                fsdoc_parse_examples(pComment, pFunction);
                
                /* Parse Return Value from the comment */
                fsdoc_parse_return_value(pComment, pFunction);
                
                fs_free(pComment, NULL);
            } else {
                pFunction->pComment = NULL;
            }

            /* Add to the list */
            if (pLastFunction == NULL) {
                pContext->pFirstFunction = pFunction;
            } else {
                pLastFunction->pNext = pFunction;
            }
            pLastFunction = pFunction;
        } else {
            fsdoc_free_function(pFunction);
        }

        fs_free(pDeclaration, NULL);
        pCurrent = pNextLine;
    }

    return 0;
}

static char* fsdoc_extract_comment_before_function(fsdoc_context* pContext, size_t functionPos)
{
    size_t commentStart, commentEnd;
    char* pComment;
    size_t commentLen;
    size_t i, j;
    char* pCleanComment;

    /* Search backwards for a comment block ending before the function */
    commentEnd = functionPos;
    
    /* Skip whitespace backwards */
    while (commentEnd > 0 && FSDOC_IS_SPACE(pContext->pFileContent[commentEnd - 1])) {
        commentEnd--;
    }

    /* Look for comment end marker */
    if (commentEnd < 2 || pContext->pFileContent[commentEnd - 2] != '*' || pContext->pFileContent[commentEnd - 1] != '/') {
        return NULL;
    }

    commentEnd -= 2;

    /* Find comment start */
    commentStart = commentEnd;
    while (commentStart > 1) {
        if (pContext->pFileContent[commentStart - 2] == '/' && pContext->pFileContent[commentStart - 1] == '*') {
            commentStart -= 2;
            break;
        }
        commentStart--;
    }

    if (commentStart >= commentEnd) {
        return NULL;
    }

    /* Extract the comment */
    commentLen = commentEnd - commentStart + 2;
    pComment = fs_malloc(commentLen + 1, NULL);
    if (pComment == NULL) {
        return NULL;
    }

    strncpy(pComment, &pContext->pFileContent[commentStart], commentLen);
    pComment[commentLen] = '\0';

    /* Clean up comment - remove comment markers and clean up formatting */
    pCleanComment = fs_malloc(commentLen + 1, NULL);
    if (pCleanComment == NULL) {
        fs_free(pComment, NULL);
        return NULL;
    }

    j = 0;
    for (i = 0; i < commentLen; i++) {
        if (i < commentLen - 1 && pComment[i] == '/' && pComment[i + 1] == '*') {
            i++; /* Skip the comment start marker */
            continue;
        }
        if (i < commentLen - 1 && pComment[i] == '*' && pComment[i + 1] == '/') {
            i++; /* Skip the comment end marker */
            continue;
        }
        if (pComment[i] == '*' && (i == 0 || pComment[i-1] == '\n')) {
            /* Skip leading asterisks on lines */
            if (i < commentLen - 1 && pComment[i + 1] == ' ') {
                i++; /* Also skip the space after the asterisk */
            }
            continue;
        }
        
        pCleanComment[j++] = pComment[i];
    }
    pCleanComment[j] = '\0';

    fs_free(pComment, NULL);
    fsdoc_trim_whitespace(pCleanComment);
    
    /* If comment is too short or looks like a section marker, ignore it */
    if (strlen(pCleanComment) < 10 || 
        strstr(pCleanComment, "BEG ") != NULL || 
        strstr(pCleanComment, "END ") != NULL) {
        fs_free(pCleanComment, NULL);
        return NULL;
    }
    
    return pCleanComment;
}

static void fsdoc_parse_description(const char* pComment, fsdoc_function* pFunction)
{
    const char* pDescStart;
    const char* pDescEnd;
    size_t descLen;
    char* pDescription;

    if (pComment == NULL || pFunction == NULL) {
        return;
    }

    /* Start from the beginning of the comment */
    pDescStart = pComment;
    
    /* Skip leading whitespace and newlines */
    while (*pDescStart == ' ' || *pDescStart == '\t' || *pDescStart == '\n') {
        pDescStart++;
    }
    
    /* Find the end of the description - stop at the first section header */
    pDescEnd = pDescStart;
    while (*pDescEnd != '\0') {
        /* Look for section headers like "Parameters", "Return Value", "Example", "See Also" */
        if (*pDescEnd == '\n') {
            const char* pLookAhead = pDescEnd + 1;
            
            /* Skip whitespace */
            while (*pLookAhead == ' ' || *pLookAhead == '\t') {
                pLookAhead++;
            }
            
            /* Check if this is the start of a section */
            if (strstr(pLookAhead, "Parameters") == pLookAhead ||
                strstr(pLookAhead, "Return Value") == pLookAhead ||
                strstr(pLookAhead, "Example") == pLookAhead ||
                strstr(pLookAhead, "See Also") == pLookAhead) {
                break;
            }
        }
        pDescEnd++;
    }
    
    /* Extract the description */
    descLen = pDescEnd - pDescStart;
    if (descLen > 0) {
        pDescription = fs_malloc(descLen + 1, NULL);
        if (pDescription != NULL) {
            strncpy(pDescription, pDescStart, descLen);
            pDescription[descLen] = '\0';
            fsdoc_normalize_indentation(pDescription);
            fsdoc_trim_whitespace(pDescription);
            
            /* Try to convert options lists to tables */
            char* pTableVersion = fsdoc_convert_options_to_table(pDescription, NULL);
            if (pTableVersion != NULL) {
                fs_free(pDescription, NULL);
                pDescription = pTableVersion;
            }
            
            if (strlen(pDescription) > 0) {
                pFunction->pDescription = pDescription;
            } else {
                fs_free(pDescription, NULL);
            }
        }
    }
}

static void fsdoc_parse_parameters_docs(const char* pComment, fsdoc_function* pFunction)
{
    const char* pParamsStart;
    const char* pParamsEnd;
    const char* pCurrent;
    const char* pLineStart;
    const char* pLineEnd;
    fsdoc_param* pParam;

    if (pComment == NULL || pFunction == NULL) {
        return;
    }

    /* Find "Parameters" section */
    pParamsStart = strstr(pComment, "Parameters");
    if (pParamsStart == NULL) {
        return;
    }

    /* Skip to the content after the dashes */
    pCurrent = pParamsStart;
    while (*pCurrent != '\0' && *pCurrent != '\n') {
        pCurrent++;
    }
    if (*pCurrent == '\n') {
        pCurrent++;
    }
    
    /* Skip dashes line */
    while (*pCurrent != '\0' && (*pCurrent == '-' || *pCurrent == ' ' || *pCurrent == '\t')) {
        pCurrent++;
    }
    if (*pCurrent == '\n') {
        pCurrent++;
    }

    /* Find the end of the parameters section */
    pParamsEnd = pCurrent;
    while (*pParamsEnd != '\0') {
        if (strncmp(pParamsEnd, "\n\n", 2) == 0) {
            const char* pLookAhead = pParamsEnd + 2;
            while (*pLookAhead == ' ' || *pLookAhead == '\t') {
                pLookAhead++;
            }
            
            if (strstr(pLookAhead, "Return Value") == pLookAhead ||
                strstr(pLookAhead, "Example") == pLookAhead ||
                strstr(pLookAhead, "See Also") == pLookAhead) {
                break;
            }
        }
        pParamsEnd++;
    }

    /* Parse each parameter entry */
    pLineStart = pCurrent;
    while (pLineStart < pParamsEnd) {
        const char* pColonPos;
        const char* pParenStart;
        const char* pParenEnd;
        const char* pDescStart;
        char paramName[128];
        char direction[32];
        char flags[64];
        char* pDescription;
        size_t nameLen;
        size_t dirLen;
        size_t flagsLen;
        size_t descLen;

        /* Find end of line */
        pLineEnd = pLineStart;
        while (pLineEnd < pParamsEnd && *pLineEnd != '\n') {
            pLineEnd++;
        }

        /* Look for parameter pattern: "paramName : (direction, flags)" */
        pColonPos = pLineStart;
        while (pColonPos < pLineEnd && *pColonPos != ':') {
            pColonPos++;
        }

        if (pColonPos < pLineEnd) {
            /* Extract parameter name */
            nameLen = pColonPos - pLineStart;
            if (nameLen > 0 && nameLen < sizeof(paramName) - 1) {
                strncpy(paramName, pLineStart, nameLen);
                paramName[nameLen] = '\0';
                fsdoc_trim_whitespace(paramName);

                /* Extract direction and flags from parentheses */
                pParenStart = pColonPos + 1;
                while (pParenStart < pLineEnd && *pParenStart != '(') {
                    pParenStart++;
                }

                if (pParenStart < pLineEnd) {
                    pParenEnd = pParenStart + 1;
                    while (pParenEnd < pLineEnd && *pParenEnd != ')') {
                        pParenEnd++;
                    }

                    if (pParenEnd < pLineEnd) {
                        const char* pContent = pParenStart + 1;
                        const char* pCommaPos = pContent;
                        
                        /* Find comma to separate direction from flags */
                        while (pCommaPos < pParenEnd && *pCommaPos != ',') {
                            pCommaPos++;
                        }

                        /* Extract direction */
                        dirLen = (pCommaPos < pParenEnd) ? (pCommaPos - pContent) : (pParenEnd - pContent);
                        if (dirLen < sizeof(direction) - 1) {
                            strncpy(direction, pContent, dirLen);
                            direction[dirLen] = '\0';
                            fsdoc_trim_whitespace(direction);
                        } else {
                            direction[0] = '\0';
                        }

                        /* Extract flags if there's a comma */
                        if (pCommaPos < pParenEnd) {
                            pContent = pCommaPos + 1;
                            flagsLen = pParenEnd - pContent;
                            if (flagsLen < sizeof(flags) - 1) {
                                strncpy(flags, pContent, flagsLen);
                                flags[flagsLen] = '\0';
                                fsdoc_trim_whitespace(flags);
                            } else {
                                flags[0] = '\0';
                            }
                        } else {
                            flags[0] = '\0';
                        }

                        /* Find and extract description (next lines until next parameter or end) */
                        pDescStart = pLineEnd + 1;
                        /* Skip only newlines, but preserve leading whitespace for indentation normalization */
                        while (pDescStart < pParamsEnd && *pDescStart == '\n') {
                            pDescStart++;
                        }

                        /* Find end of description */
                        const char* pDescEnd = pDescStart;
                        while (pDescEnd < pParamsEnd) {
                            if (*pDescEnd == '\n') {
                                const char* pNextLine = pDescEnd + 1;
                                /* Skip whitespace */
                                while (pNextLine < pParamsEnd && (*pNextLine == ' ' || *pNextLine == '\t')) {
                                    pNextLine++;
                                }
                                /* Check if next line starts a new parameter (contains ':') */
                                const char* pCheckColon = pNextLine;
                                while (pCheckColon < pParamsEnd && *pCheckColon != '\n' && *pCheckColon != ':') {
                                    pCheckColon++;
                                }
                                if (pCheckColon < pParamsEnd && *pCheckColon == ':') {
                                    break; /* Found next parameter */
                                }
                            }
                            pDescEnd++;
                        }

                        /* Extract description */
                        descLen = pDescEnd - pDescStart;
                        if (descLen > 0) {
                            pDescription = fs_malloc(descLen + 1, NULL);
                            if (pDescription != NULL) {
                                strncpy(pDescription, pDescStart, descLen);
                                pDescription[descLen] = '\0';
                                fsdoc_normalize_indentation(pDescription);
                                fsdoc_trim_whitespace(pDescription);
                                
                                /* Try to convert options lists to tables */
                                char* pTableVersion = fsdoc_convert_options_to_table(pDescription, NULL);
                                if (pTableVersion != NULL) {
                                    fs_free(pDescription, NULL);
                                    pDescription = pTableVersion;
                                }
                            }
                        } else {
                            pDescription = NULL;
                        }

                        /* Find matching parameter in function and update it */
                        for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
                            if (strcmp(pParam->name, paramName) == 0) {
                                strcpy(pParam->direction, direction);
                                strcpy(pParam->flags, flags);
                                pParam->pDescription = pDescription;
                                pParam->isDocumented = 1;
                                break;
                            }
                        }

                        /* If no matching parameter found, print warning */
                        if (pParam == NULL) {
                            printf("Warning: Parameter '%s' documented but not found in function declaration for %s\n", 
                                   paramName, pFunction->name);
                            if (pDescription != NULL) {
                                fs_free(pDescription, NULL);
                            }
                        }

                        /* Move to next parameter */
                        pLineStart = pDescEnd;
                        continue;
                    }
                }
            }
        }

        /* Move to next line if we couldn't parse this one */
        pLineStart = pLineEnd + 1;
    }

    /* Check for undocumented parameters */
    for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
        if (!pParam->isDocumented && strlen(pParam->name) > 0) {
            printf("Warning: Parameter '%s' in function declaration but not documented for %s\n", 
                   pParam->name, pFunction->name);
        }
    }
}

static void fsdoc_parse_see_also(const char* pComment, fsdoc_function* pFunction)
{
    const char* pSeeAlsoStart;
    const char* pSeeAlsoEnd;
    const char* pCurrent;
    const char* pLineStart;
    const char* pLineEnd;
    fsdoc_see_also* pLastSeeAlso;

    if (pComment == NULL || pFunction == NULL) {
        return;
    }

    /* Find "See Also" section */
    pSeeAlsoStart = strstr(pComment, "See Also");
    if (pSeeAlsoStart == NULL) {
        return;
    }

    /* Skip the "See Also" header and dashes */
    pSeeAlsoStart = strchr(pSeeAlsoStart, '\n');
    if (pSeeAlsoStart == NULL) {
        return;
    }
    pSeeAlsoStart++; /* Move past the newline */

    /* Skip any dashes */
    while (*pSeeAlsoStart == '-' || *pSeeAlsoStart == '\n' || *pSeeAlsoStart == ' ') {
        pSeeAlsoStart++;
    }

    /* Find the end of the See Also section (next section or end of comment) */
    pSeeAlsoEnd = strstr(pSeeAlsoStart, "\n\n");
    if (pSeeAlsoEnd == NULL) {
        pSeeAlsoEnd = pSeeAlsoStart + strlen(pSeeAlsoStart);
    }

    /* Parse each line in the See Also section */
    pLastSeeAlso = NULL;
    pCurrent = pSeeAlsoStart;
    
    while (pCurrent < pSeeAlsoEnd) {
        /* Find start and end of current line */
        pLineStart = pCurrent;
        pLineEnd = strchr(pLineStart, '\n');
        if (pLineEnd == NULL || pLineEnd > pSeeAlsoEnd) {
            pLineEnd = pSeeAlsoEnd;
        }

        /* Skip empty lines */
        if (pLineEnd > pLineStart) {
            char line[256];
            size_t lineLen = pLineEnd - pLineStart;
            
            if (lineLen < sizeof(line)) {
                strncpy(line, pLineStart, lineLen);
                line[lineLen] = '\0';
                fsdoc_trim_whitespace(line);
                
                /* If line is not empty, add it as a see also entry */
                if (strlen(line) > 0 && !strstr(line, "----")) {
                    fsdoc_see_also* pSeeAlso = fs_malloc(sizeof(fsdoc_see_also), NULL);
                    if (pSeeAlso != NULL) {
                        memset(pSeeAlso, 0, sizeof(*pSeeAlso));
                        strncpy(pSeeAlso->name, line, sizeof(pSeeAlso->name) - 1);
                        pSeeAlso->name[sizeof(pSeeAlso->name) - 1] = '\0';
                        
                        if (pLastSeeAlso == NULL) {
                            pFunction->pFirstSeeAlso = pSeeAlso;
                        } else {
                            pLastSeeAlso->pNext = pSeeAlso;
                        }
                        pLastSeeAlso = pSeeAlso;
                    }
                }
            }
        }

        pCurrent = pLineEnd + 1;
    }
}

static void fsdoc_parse_examples(const char* pComment, fsdoc_function* pFunction)
{
    const char* pCurrent;
    const char* pExampleStart;
    fsdoc_example* pLastExample;

    if (pComment == NULL || pFunction == NULL) {
        return;
    }

    pLastExample = NULL;
    pCurrent = pComment;

    /* Look for example headers like "Example 1 - Basic Usage" */
    while ((pExampleStart = strstr(pCurrent, "Example ")) != NULL) {
        const char* pTitleEnd;
        const char* pContentStart;
        const char* pContentEnd;
        const char* pNextExample;
        char title[256];
        char* pContent;
        size_t titleLen;
        size_t contentLen;

        /* Find the end of the title line */
        pTitleEnd = strchr(pExampleStart, '\n');
        if (pTitleEnd == NULL) {
            break;
        }

        /* Extract the title */
        titleLen = pTitleEnd - pExampleStart;
        if (titleLen >= sizeof(title)) {
            titleLen = sizeof(title) - 1;
        }
        strncpy(title, pExampleStart, titleLen);
        title[titleLen] = '\0';
        fsdoc_trim_whitespace(title);

        /* Skip the dashes line */
        pContentStart = pTitleEnd + 1;
        while (*pContentStart == '-' || *pContentStart == '\n' || *pContentStart == ' ') {
            pContentStart++;
        }

        /* Find the end of this example - look for the next "Example" or end of comment */
        pNextExample = strstr(pContentStart, "\nExample ");
        if (pNextExample != NULL) {
            pContentEnd = pNextExample;
        } else {
            /* Check for other section markers that might end the example */
            const char* pSeeAlso = strstr(pContentStart, "\n\nSee Also");
            if (pSeeAlso != NULL) {
                pContentEnd = pSeeAlso;
            } else {
                pContentEnd = pContentStart + strlen(pContentStart);
            }
        }

        /* Extract the content */
        contentLen = pContentEnd - pContentStart;
        pContent = fs_malloc(contentLen + 1, NULL);
        if (pContent != NULL) {
            strncpy(pContent, pContentStart, contentLen);
            pContent[contentLen] = '\0';
            fsdoc_trim_whitespace(pContent);

            /* Create the example entry */
            if (strlen(pContent) > 0) {
                fsdoc_example* pExample = fs_malloc(sizeof(fsdoc_example), NULL);
                if (pExample != NULL) {
                    memset(pExample, 0, sizeof(*pExample));
                    strncpy(pExample->title, title, sizeof(pExample->title) - 1);
                    pExample->title[sizeof(pExample->title) - 1] = '\0';
                    pExample->pContent = pContent;

                    if (pLastExample == NULL) {
                        pFunction->pFirstExample = pExample;
                    } else {
                        pLastExample->pNext = pExample;
                    }
                    pLastExample = pExample;
                } else {
                    fs_free(pContent, NULL);
                }
            } else {
                fs_free(pContent, NULL);
            }
        }

        /* Move to the next potential example */
        if (pNextExample != NULL) {
            pCurrent = pNextExample + 1;
        } else {
            break;
        }
    }
}

static void fsdoc_parse_return_value(const char* pComment, fsdoc_function* pFunction)
{
    const char* pReturnStart;
    const char* pContentStart;
    const char* pContentEnd;
    size_t contentLen;
    char* pReturnValue;

    if (pComment == NULL || pFunction == NULL) {
        return;
    }

    /* Look for "Return Value" section */
    pReturnStart = strstr(pComment, "Return Value");
    if (pReturnStart == NULL) {
        return;
    }

    /* Find the line after the dashes */
    pContentStart = pReturnStart;
    while (*pContentStart != '\0' && *pContentStart != '\n') {
        pContentStart++;
    }
    if (*pContentStart == '\n') {
        pContentStart++;
    }
    
    /* Skip dashes line */
    while (*pContentStart != '\0' && (*pContentStart == '-' || *pContentStart == ' ' || *pContentStart == '\t')) {
        pContentStart++;
    }
    if (*pContentStart == '\n') {
        pContentStart++;
    }

    /* Find the end of the return value section (next section or end of comment) */
    pContentEnd = pContentStart;
    while (*pContentEnd != '\0') {
        if (strncmp(pContentEnd, "\n\n", 2) == 0) {
            /* Look ahead to see if this is the start of a new section */
            const char* pLookAhead = pContentEnd + 2;
            
            /* Skip whitespace */
            while (*pLookAhead == ' ' || *pLookAhead == '\t') {
                pLookAhead++;
            }
            
            /* Check if this is a new section (starts with a capital letter followed by text and dashes) */
            if (strstr(pLookAhead, "Example") == pLookAhead ||
                strstr(pLookAhead, "Parameters") == pLookAhead ||
                strstr(pLookAhead, "See Also") == pLookAhead ||
                strstr(pLookAhead, "*") == pLookAhead) {
                break;
            }
        }
        pContentEnd++;
    }

    /* Extract the return value content */
    contentLen = pContentEnd - pContentStart;
    if (contentLen > 0) {
        pReturnValue = fs_malloc(contentLen + 1, NULL);
        if (pReturnValue != NULL) {
            strncpy(pReturnValue, pContentStart, contentLen);
            pReturnValue[contentLen] = '\0';
            fsdoc_normalize_indentation(pReturnValue);
            fsdoc_trim_whitespace(pReturnValue);
            
            /* Try to convert options lists to tables */
            char* pTableVersion = fsdoc_convert_options_to_table(pReturnValue, NULL);
            if (pTableVersion != NULL) {
                fs_free(pReturnValue, NULL);
                pReturnValue = pTableVersion;
            }
            
            if (strlen(pReturnValue) > 0) {
                pFunction->pReturnValue = pReturnValue;
            } else {
                fs_free(pReturnValue, NULL);
            }
        }
    }
}

static int fsdoc_parse_function_declaration(const char* pDeclaration, fsdoc_function* pFunction)
{
    char* pWorkingCopy;
    char* pFuncStart;
    char* pParenOpen;
    char* pParenClose;
    char* pToken;
    fsdoc_param* pLastParam;

    if (pDeclaration == NULL || pFunction == NULL) {
        return 1;
    }

    pWorkingCopy = fs_malloc(strlen(pDeclaration) + 1, NULL);
    if (pWorkingCopy == NULL) {
        return 1;
    }
    strcpy(pWorkingCopy, pDeclaration);

    /* Remove "FS_API" prefix */
    pFuncStart = strstr(pWorkingCopy, "FS_API");
    if (pFuncStart == NULL) {
        fs_free(pWorkingCopy, NULL);
        return 1;
    }
    pFuncStart += 6; /* Skip "FS_API" */

    /* Find the opening parenthesis */
    pParenOpen = strchr(pFuncStart, '(');
    if (pParenOpen == NULL) {
        fs_free(pWorkingCopy, NULL);
        return 1;
    }

    /* Find the closing parenthesis (first one after opening, not last one in string) */
    pParenClose = pParenOpen;
    int parenCount = 1;
    pParenClose++; /* Skip the opening parenthesis */
    while (*pParenClose != '\0' && parenCount > 0) {
        if (*pParenClose == '(') {
            parenCount++;
        } else if (*pParenClose == ')') {
            parenCount--;
        }
        if (parenCount > 0) {
            pParenClose++;
        }
    }
    if (parenCount != 0) {
        fs_free(pWorkingCopy, NULL);
        return 1;
    }

    /* Clean up any attributes after the closing parenthesis */
    char* pAttributeStart = pParenClose + 1;
    while (*pAttributeStart == ' ' || *pAttributeStart == '\t') {
        pAttributeStart++;
    }
    if (strstr(pAttributeStart, "FS_ATTRIBUTE_FORMAT") == pAttributeStart) {
        *pParenClose = '\0';
        /* Find the actual closing parenthesis before the attribute */
        char* pRealClose = pParenClose - 1;
        while (pRealClose > pParenOpen && *pRealClose != ')') {
            pRealClose--;
        }
        if (*pRealClose == ')') {
            pParenClose = pRealClose;
        }
    }

    /* Null-terminate at the opening parenthesis to extract return type and function name */
    *pParenOpen = '\0';

    /* Parse return type and function name */
    fsdoc_trim_whitespace(pFuncStart);
    
    /* Find the last token, which should be the function name */
    pToken = strrchr(pFuncStart, ' ');
    if (pToken != NULL) {
        /* We have a return type */
        *pToken = '\0';
        pToken++;
        
        /* Extract return type */
        fsdoc_trim_whitespace(pFuncStart);
        strncpy(pFunction->return_type, pFuncStart, sizeof(pFunction->return_type) - 1);
        
        /* Extract function name */
        fsdoc_trim_whitespace(pToken);
        strncpy(pFunction->name, pToken, sizeof(pFunction->name) - 1);
    } else {
        /* No return type, just function name */
        strncpy(pFunction->name, pFuncStart, sizeof(pFunction->name) - 1);
        strcpy(pFunction->return_type, "void");
    }

    /* Parse parameters */
    *pParenClose = '\0';
    pParenOpen++; /* Move past the opening parenthesis */
    
    /* Clean up any attributes that might have been included */
    char* pAttrPos = strstr(pParenOpen, ") FS_ATTRIBUTE_FORMAT");
    if (pAttrPos != NULL) {
        *pAttrPos = '\0';
    }
    
    fsdoc_trim_whitespace(pParenOpen);
    
    if (strlen(pParenOpen) > 0 && strcmp(pParenOpen, "void") != 0) {
        /* Custom parameter parsing to handle function pointers correctly */
        char* pParamStart = pParenOpen;
        char* pParamEnd;
        int parenDepth;
        
        pLastParam = NULL;
        
        while (*pParamStart != '\0') {
            /* Skip leading whitespace */
            while (*pParamStart == ' ' || *pParamStart == '\t') {
                pParamStart++;
            }
            
            if (*pParamStart == '\0') {
                break;
            }
            
            /* Find the end of this parameter, considering nested parentheses */
            pParamEnd = pParamStart;
            parenDepth = 0;
            
            while (*pParamEnd != '\0') {
                if (*pParamEnd == '(') {
                    parenDepth++;
                } else if (*pParamEnd == ')') {
                    parenDepth--;
                } else if (*pParamEnd == ',' && parenDepth == 0) {
                    break;
                }
                pParamEnd++;
            }
            
            /* Extract the parameter text */
            char paramText[256];
            size_t paramLen = pParamEnd - pParamStart;
            if (paramLen >= sizeof(paramText)) {
                paramLen = sizeof(paramText) - 1;
            }
            strncpy(paramText, pParamStart, paramLen);
            paramText[paramLen] = '\0';
            
            fsdoc_trim_whitespace(paramText);
            
            if (strlen(paramText) > 0) {
                fsdoc_param* pParam = fs_malloc(sizeof(fsdoc_param), NULL);
                if (pParam == NULL) {
                    fs_free(pWorkingCopy, NULL);
                    return 1;
                }
                
                memset(pParam, 0, sizeof(*pParam));
                
                /* Initialize new fields */
                strcpy(pParam->direction, "");
                strcpy(pParam->flags, "");
                pParam->pDescription = NULL;
                pParam->isDocumented = 0;
                
                /* Special case for variadic parameters */
                if (strstr(paramText, "...") != NULL) {
                    strcpy(pParam->type, "...");
                    strcpy(pParam->name, "");
                } else {
                    /* Check for function pointer pattern */
                    char* pFuncPtr = strstr(paramText, "(*");
                    if (pFuncPtr != NULL) {
                        /* Function pointer parameter */
                        char* pNameStart = pFuncPtr + 2;
                        char* pNameEnd = strchr(pNameStart, ')');
                        if (pNameEnd != NULL) {
                            /* Extract function pointer name */
                            size_t nameLen = pNameEnd - pNameStart;
                            if (nameLen < sizeof(pParam->name)) {
                                strncpy(pParam->name, pNameStart, nameLen);
                                pParam->name[nameLen] = '\0';
                            } else {
                                strcpy(pParam->name, "param");
                            }
                            
                            /* The entire paramText is the type */
                            strncpy(pParam->type, paramText, sizeof(pParam->type) - 1);
                        } else {
                            /* Fallback */
                            strncpy(pParam->type, paramText, sizeof(pParam->type) - 1);
                            strcpy(pParam->name, "param");
                        }
                    } else {
                        /* Regular parameter - find the parameter name (last token) */
                        char* pParamName = strrchr(paramText, ' ');
                        if (pParamName != NULL) {
                            *pParamName = '\0';
                            pParamName++;
                            
                            /* Remove pointer stars from name */
                            while (*pParamName == '*') {
                                pParamName++;
                            }
                            
                            strncpy(pParam->name, pParamName, sizeof(pParam->name) - 1);
                            strncpy(pParam->type, paramText, sizeof(pParam->type) - 1);
                        } else {
                            strncpy(pParam->type, paramText, sizeof(pParam->type) - 1);
                            strcpy(pParam->name, "param");
                        }
                    }
                }
                
                /* Add to list */
                if (pLastParam == NULL) {
                    pFunction->pFirstParam = pParam;
                } else {
                    pLastParam->pNext = pParam;
                }
                pLastParam = pParam;
            }
            
            /* Move to next parameter */
            if (*pParamEnd == ',') {
                pParamStart = pParamEnd + 1;
            } else {
                break;
            }
        }
    }

    fs_free(pWorkingCopy, NULL);
    return 0;
}

static void fsdoc_write_json_string(fs_file* pFile, const char* pStr)
{
    const char* p;
    
    if (pStr == NULL) {
        fs_file_write(pFile, "\"\"", 2, NULL);
        return;
    }
    
    fs_file_write(pFile, "\"", 1, NULL);
    for (p = pStr; *p != '\0'; p++) {
        switch (*p) {
            case '"':  fs_file_write(pFile, "\\\"", 2, NULL); break;
            case '\\': fs_file_write(pFile, "\\\\", 2, NULL); break;
            case '\n': fs_file_write(pFile, "\\n", 2, NULL); break;
            case '\r': fs_file_write(pFile, "\\r", 2, NULL); break;
            case '\t': fs_file_write(pFile, "\\t", 2, NULL); break;
            default:   fs_file_write(pFile, p, 1, NULL); break;
        }
    }
    fs_file_write(pFile, "\"", 1, NULL);
}

static int fsdoc_output_json(fsdoc_context* pContext, const char* pOutputPath)
{
    fs_file* pFile;
    fs_result result;
    fsdoc_function* pFunction;
    fsdoc_param* pParam;
    fsdoc_see_also* pSeeAlso;
    fsdoc_example* pExample;
    int isFirstFunction;
    int isFirstParam;
    int isFirstSeeAlso;
    int isFirstExample;

    if (pContext == NULL || pOutputPath == NULL) {
        return 1;
    }

    result = fs_file_open(NULL, pOutputPath, FS_WRITE | FS_TRUNCATE, &pFile);
    if (result != FS_SUCCESS) {
        return 1;
    }

    fs_file_writef(pFile, "{\n");
    fs_file_writef(pFile, "  \"functions\": [\n");

    isFirstFunction = 1;
    for (pFunction = pContext->pFirstFunction; pFunction != NULL; pFunction = pFunction->pNext) {
        if (!isFirstFunction) {
            fs_file_writef(pFile, ",\n");
        }
        isFirstFunction = 0;

        fs_file_writef(pFile, "    {\n");
        fs_file_writef(pFile, "      \"name\": ");
        fsdoc_write_json_string(pFile, pFunction->name);
        fs_file_writef(pFile, ",\n");
        
        fs_file_writef(pFile, "      \"return_type\": ");
        fsdoc_write_json_string(pFile, pFunction->return_type);
        fs_file_writef(pFile, ",\n");
        
        fs_file_writef(pFile, "      \"comment\": ");
        fsdoc_write_json_string(pFile, pFunction->pComment);
        fs_file_writef(pFile, ",\n");
        
        fs_file_writef(pFile, "      \"description\": ");
        fsdoc_write_json_string(pFile, pFunction->pDescription);
        fs_file_writef(pFile, ",\n");
        
        fs_file_writef(pFile, "      \"return_value\": ");
        fsdoc_write_json_string(pFile, pFunction->pReturnValue);
        fs_file_writef(pFile, ",\n");
        
        fs_file_writef(pFile, "      \"parameters\": [\n");

        isFirstParam = 1;
        for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
            if (!isFirstParam) {
                fs_file_writef(pFile, ",\n");
            }
            isFirstParam = 0;

            fs_file_writef(pFile, "        {\n");
            fs_file_writef(pFile, "          \"type\": ");
            fsdoc_write_json_string(pFile, pParam->type);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"name\": ");
            fsdoc_write_json_string(pFile, pParam->name);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"direction\": ");
            fsdoc_write_json_string(pFile, pParam->direction);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"flags\": ");
            fsdoc_write_json_string(pFile, pParam->flags);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"description\": ");
            fsdoc_write_json_string(pFile, pParam->pDescription);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"is_documented\": %s\n", pParam->isDocumented ? "true" : "false");
            fs_file_writef(pFile, "        }");
        }

        fs_file_writef(pFile, "\n      ],\n");
        
        fs_file_writef(pFile, "      \"see_also\": [\n");
        
        isFirstSeeAlso = 1;
        for (pSeeAlso = pFunction->pFirstSeeAlso; pSeeAlso != NULL; pSeeAlso = pSeeAlso->pNext) {
            if (!isFirstSeeAlso) {
                fs_file_writef(pFile, ",\n");
            }
            isFirstSeeAlso = 0;

            fs_file_writef(pFile, "        ");
            fsdoc_write_json_string(pFile, pSeeAlso->name);
        }

        fs_file_writef(pFile, "\n      ],\n");
        
        fs_file_writef(pFile, "      \"examples\": [\n");
        
        isFirstExample = 1;
        for (pExample = pFunction->pFirstExample; pExample != NULL; pExample = pExample->pNext) {
            if (!isFirstExample) {
                fs_file_writef(pFile, ",\n");
            }
            isFirstExample = 0;

            fs_file_writef(pFile, "        {\n");
            fs_file_writef(pFile, "          \"title\": ");
            fsdoc_write_json_string(pFile, pExample->title);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"content\": ");
            fsdoc_write_json_string(pFile, pExample->pContent);
            fs_file_writef(pFile, "\n");
            fs_file_writef(pFile, "        }");
        }

        fs_file_writef(pFile, "\n      ]\n");
        fs_file_writef(pFile, "    }");
    }

    fs_file_writef(pFile, "\n  ]\n");
    fs_file_writef(pFile, "}\n");

    fs_file_close(pFile);
    return 0;
}

static int fsdoc_output_markdown(fsdoc_context* pContext, const char* pOutputPath)
{
    fs_file* pFile;
    fs_result result;
    fsdoc_function* pFunction;
    fsdoc_param* pParam;
    fsdoc_see_also* pSeeAlso;
    fsdoc_example* pExample;

    if (pContext == NULL || pOutputPath == NULL) {
        return 1;
    }

    result = fs_file_open(NULL, pOutputPath, FS_WRITE | FS_TRUNCATE, &pFile);
    if (result != FS_SUCCESS) {
        return 1;
    }

    fs_file_writef(pFile, "# FS API Documentation\n\n");
    fs_file_writef(pFile, "---\n\n");

    for (pFunction = pContext->pFirstFunction; pFunction != NULL; pFunction = pFunction->pNext) {
        /* Function name as main header */
        fs_file_writef(pFile, "# %s\n\n", pFunction->name);
        
        /* Function signature with nicely aligned parameters */
        fs_file_writef(pFile, "```c\n%s %s(\n", pFunction->return_type, pFunction->name);
        
        if (pFunction->pFirstParam != NULL) {
            /* First pass: calculate the maximum width for alignment */
            int maxDirectionWidth = 0;
            int maxTypeWidth = 0;
            
            for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
                int directionWidth = 0;
                int typeWidth = (int)strlen(pParam->type);
                
                if (pParam->isDocumented && strlen(pParam->direction) > 0) {
                    directionWidth = (int)strlen(pParam->direction) + 2; /* +2 for brackets */
                    if (strlen(pParam->flags) > 0) {
                        directionWidth += (int)strlen(pParam->flags) + 2; /* +2 for ", " */
                    }
                    directionWidth += 1; /* +1 for space after bracket */
                }
                
                if (directionWidth > maxDirectionWidth) {
                    maxDirectionWidth = directionWidth;
                }
                if (typeWidth > maxTypeWidth) {
                    maxTypeWidth = typeWidth;
                }
            }
            
            /* Second pass: write parameters with alignment */
            for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
                fs_file_writef(pFile, "    ");
                
                /* Add direction/flags in brackets if documented */
                int currentDirectionWidth = 0;
                if (pParam->isDocumented && strlen(pParam->direction) > 0) {
                    fs_file_writef(pFile, "[%s", pParam->direction);
                    currentDirectionWidth = (int)strlen(pParam->direction) + 2;
                    if (strlen(pParam->flags) > 0) {
                        fs_file_writef(pFile, ", %s", pParam->flags);
                        currentDirectionWidth += (int)strlen(pParam->flags) + 2;
                    }
                    fs_file_writef(pFile, "] ");
                    currentDirectionWidth += 1;
                }
                
                /* Pad to align direction/flags */
                while (currentDirectionWidth < maxDirectionWidth) {
                    fs_file_writef(pFile, " ");
                    currentDirectionWidth++;
                }
                
                /* Write type and pad to align */
                fs_file_writef(pFile, "%s", pParam->type);
                int currentTypeWidth = (int)strlen(pParam->type);
                while (currentTypeWidth < maxTypeWidth) {
                    fs_file_writef(pFile, " ");
                    currentTypeWidth++;
                }
                
                fs_file_writef(pFile, " %s", pParam->name);
                
                /* Add comma if not the last parameter */
                if (pParam->pNext != NULL) {
                    fs_file_writef(pFile, ",");
                }
                
                fs_file_writef(pFile, "\n");
            }
        }
        
        fs_file_writef(pFile, ");\n```\n\n");
        
        /* Check if function is documented */
        int isDocumented = 0;
        int hasDocumentedParams = 0;
        
        /* Check if function has description */
        if (pFunction->pDescription != NULL && strlen(pFunction->pDescription) > 0) {
            isDocumented = 1;
        }
        
        /* Check if any parameters have descriptions or are documented with direction/flags */
        for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
            if ((pParam->pDescription != NULL && strlen(pParam->pDescription) > 0) || 
                (pParam->isDocumented && strlen(pParam->direction) > 0)) {
                isDocumented = 1;
                hasDocumentedParams = 1;
                break;
            }
        }
        
        /* Check if function has return value description */
        if (!isDocumented && pFunction->pReturnValue != NULL && strlen(pFunction->pReturnValue) > 0) {
            isDocumented = 1;
        }
        
        /* Check if function has examples */
        if (!isDocumented && pFunction->pFirstExample != NULL) {
            isDocumented = 1;
        }
        
        /* Check if function has see also entries */
        if (!isDocumented && pFunction->pFirstSeeAlso != NULL) {
            isDocumented = 1;
        }
        
        /* If function is undocumented, skip all sections */
        if (!isDocumented) {
            fs_file_writef(pFile, "---\n\n");
            continue;
        }
        
        /* Description */
        if (pFunction->pDescription != NULL && strlen(pFunction->pDescription) > 0) {
            fs_file_writef(pFile, "%s\n\n", pFunction->pDescription);
        }
        
        /* Parameters section - only show if there are documented parameters */
        if (pFunction->pFirstParam != NULL && hasDocumentedParams) {
            fs_file_writef(pFile, "## Parameters\n\n");
            for (pParam = pFunction->pFirstParam; pParam != NULL; pParam = pParam->pNext) {
                /* Parameter name with direction/flags */
                if (pParam->isDocumented && strlen(pParam->direction) > 0) {
                    fs_file_writef(pFile, "[%s", pParam->direction);
                    if (strlen(pParam->flags) > 0) {
                        fs_file_writef(pFile, ", %s", pParam->flags);
                    }
                    fs_file_writef(pFile, "] ");
                }
                fs_file_writef(pFile, "**%s**  \n", pParam->name);
                
                /* Parameter description on new line */
                if (pParam->pDescription != NULL && strlen(pParam->pDescription) > 0) {
                    fs_file_writef(pFile, "%s\n\n", pParam->pDescription);
                } else {
                    fs_file_writef(pFile, "\n");
                }
            }
        } else if (pFunction->pFirstParam == NULL) {
            /* Show "None" only if function has no parameters at all */
            fs_file_writef(pFile, "## Parameters\n\n");
            fs_file_writef(pFile, "None\n\n");
        }
        
        /* Return Value section - only show if there's content */
        if (pFunction->pReturnValue != NULL && strlen(pFunction->pReturnValue) > 0) {
            fs_file_writef(pFile, "## Return Value\n\n");
            fs_file_writef(pFile, "%s\n\n", pFunction->pReturnValue);
        }
        
        /* Examples section - only show if there are examples */
        if (pFunction->pFirstExample != NULL) {
            for (pExample = pFunction->pFirstExample; pExample != NULL; pExample = pExample->pNext) {
                fs_file_writef(pFile, "## %s\n\n", pExample->title);
                fs_file_writef(pFile, "%s\n\n", pExample->pContent);
            }
        }
        
        /* See Also section - only show if there are see also entries */
        if (pFunction->pFirstSeeAlso != NULL) {
            fs_file_writef(pFile, "## See Also\n\n");
            for (pSeeAlso = pFunction->pFirstSeeAlso; pSeeAlso != NULL; pSeeAlso = pSeeAlso->pNext) {
                /* Create link target by removing "()" if present */
                char linkTarget[256];
                strncpy(linkTarget, pSeeAlso->name, sizeof(linkTarget) - 1);
                linkTarget[sizeof(linkTarget) - 1] = '\0';
                
                char* pParens = strstr(linkTarget, "()");
                if (pParens != NULL) {
                    *pParens = '\0';  /* Remove "()" from link target */
                }
                
                fs_file_writef(pFile, "[%s](#%s)  \n", pSeeAlso->name, linkTarget);  /* Two spaces before \n creates a line break instead of paragraph break */
            }
            fs_file_writef(pFile, "\n");
        }
        
        fs_file_writef(pFile, "---\n\n");
    }

    fs_file_close(pFile);
    return 0;
}

static void fsdoc_normalize_indentation(char* pStr)
{
    char* pLine;
    int minIndent;
    int currentIndent;
    char* pWritePos;
    char* pReadPos;

    if (pStr == NULL || strlen(pStr) == 0) {
        return;
    }

    /* First pass: find the minimum indentation level (excluding empty lines) */
    minIndent = -1; /* -1 means not set yet */
    pLine = pStr;
    
    while (*pLine != '\0') {
        /* Skip empty lines */
        if (*pLine == '\n') {
            pLine++;
            continue;
        }
        
        /* Count leading spaces/tabs */
        currentIndent = 0;
        char* pChar = pLine;
        while (*pChar == ' ' || *pChar == '\t') {
            currentIndent++;
            pChar++;
        }
        
        /* Skip if this is an empty line (only whitespace) */
        if (*pChar == '\n' || *pChar == '\0') {
            /* Find next line */
            while (*pLine != '\0' && *pLine != '\n') {
                pLine++;
            }
            if (*pLine == '\n') {
                pLine++;
            }
            continue;
        }
        
        /* Update minimum indentation */
        if (minIndent == -1 || currentIndent < minIndent) {
            minIndent = currentIndent;
        }
        
        /* Find next line */
        while (*pLine != '\0' && *pLine != '\n') {
            pLine++;
        }
        if (*pLine == '\n') {
            pLine++;
        }
    }
    
    /* If no indentation found or minimum is 0, nothing to do */
    if (minIndent <= 0) {
        return;
    }
    
    /* Second pass: remove the minimum indentation from each line */
    pWritePos = pStr;
    pReadPos = pStr;
    
    while (*pReadPos != '\0') {
        char* pLineStart;
        int indentToSkip;
        int i;
        
        /* Handle newlines */
        if (*pReadPos == '\n') {
            *pWritePos = *pReadPos;
            pWritePos++;
            pReadPos++;
            continue;
        }
        
        /* Count current line's indentation */
        currentIndent = 0;
        pLineStart = pReadPos;
        while (*pReadPos == ' ' || *pReadPos == '\t') {
            currentIndent++;
            pReadPos++;
        }
        
        /* If this is an empty line (only whitespace), copy as-is */
        if (*pReadPos == '\n' || *pReadPos == '\0') {
            while (pLineStart <= pReadPos) {
                *pWritePos = *pLineStart;
                pWritePos++;
                pLineStart++;
            }
            continue;
        }
        
        /* Skip the minimum indentation amount */
        pReadPos = pLineStart;
        indentToSkip = (currentIndent >= minIndent) ? minIndent : currentIndent;
        for (i = 0; i < indentToSkip; i++) {
            if (*pReadPos == ' ' || *pReadPos == '\t') {
                pReadPos++;
            }
        }
        
        /* Copy the rest of the line */
        while (*pReadPos != '\0' && *pReadPos != '\n') {
            *pWritePos = *pReadPos;
            pWritePos++;
            pReadPos++;
        }
    }
    
    *pWritePos = '\0';
}

static char* fsdoc_convert_options_to_table(const char* pStr, const fs_allocation_callbacks* pAllocationCallbacks)
{
    const char* pCurrent;
    const char* pLine;
    const char* pLineEnd;
    char* pResult;
    char* pWrite;
    size_t resultSize;
    int hasOptions;
    int hasDescriptions;
    int lineLen;
    char line[512];

    if (pStr == NULL) {
        return NULL;
    }

    /* First, check if this looks like an options list and if any options have descriptions */
    hasOptions = 0;
    hasDescriptions = 0;
    pCurrent = pStr;
    
    /* Look for pattern: text followed by uppercase identifiers with descriptions */
    while (*pCurrent != '\0') {
        pLine = pCurrent;
        pLineEnd = strchr(pLine, '\n');
        if (pLineEnd == NULL) {
            pLineEnd = pLine + strlen(pLine);
        }
        
        lineLen = (int)(pLineEnd - pLine);
        if (lineLen > 0 && lineLen < (int)(sizeof(line) - 1)) {
            strncpy(line, pLine, lineLen);
            line[lineLen] = '\0';
            fsdoc_trim_whitespace(line);
            
            /* Check if this looks like an option (starts with uppercase letters/underscores) */
            if (strlen(line) > 0) {
                const char* pChar = line;
                int isOption = 1;
                
                /* First character should be uppercase or underscore */
                if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_') {
                    isOption = 0;
                }
                
                /* Rest should be uppercase, underscore, or digit */
                pChar++;
                while (*pChar != '\0' && isOption) {
                    if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_' && !FSDOC_IS_DIGIT(*pChar)) {
                        isOption = 0;
                    }
                    pChar++;
                }
                
                if (isOption && strlen(line) > 2) {
                    hasOptions = 1;
                    
                    /* Check if this option has a description */
                    const char* pCheckCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
                    while (*pCheckCurrent != '\0') {
                        const char* pDescLine = pCheckCurrent;
                        const char* pDescEnd = strchr(pDescLine, '\n');
                        if (pDescEnd == NULL) {
                            pDescEnd = pDescLine + strlen(pDescLine);
                        }
                        
                        int descLen = (int)(pDescEnd - pDescLine);
                        if (descLen > 0 && descLen < (int)(sizeof(line) - 1)) {
                            char descBuffer[512];
                            strncpy(descBuffer, pDescLine, descLen);
                            descBuffer[descLen] = '\0';
                            fsdoc_trim_whitespace(descBuffer);
                            
                            if (strlen(descBuffer) == 0) {
                                /* Empty line, move to next */
                                pCheckCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                                continue;
                            }
                            
                            /* Check if this is the next option */
                            const char* pCheck = descBuffer;
                            int isNextOption = 1;
                            if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_') {
                                isNextOption = 0;
                            } else {
                                pCheck++;
                                while (*pCheck != '\0' && isNextOption) {
                                    if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_' && !FSDOC_IS_DIGIT(*pCheck)) {
                                        isNextOption = 0;
                                    }
                                    pCheck++;
                                }
                            }
                            
                            if (isNextOption && strlen(descBuffer) > 2) {
                                /* This is the next option, stop checking */
                                break;
                            } else {
                                /* This is a description line */
                                hasDescriptions = 1;
                                break;
                            }
                        }
                        
                        pCheckCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                    }
                    
                    if (hasDescriptions) {
                        break; /* Found at least one description, no need to check further */
                    }
                }
            }
        }
        
        pCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
    }
    
    if (!hasOptions) {
        return NULL; /* Not an options list, return NULL to use original text */
    }
    
    /* Allocate result buffer (generous size estimate) */
    resultSize = strlen(pStr) * 2 + 1024;
    pResult = fs_malloc(resultSize, pAllocationCallbacks);
    if (pResult == NULL) {
        return NULL;
    }
    
    pWrite = pResult;
    pCurrent = pStr;
    
    /* First, copy any introductory text before the first option */
    while (*pCurrent != '\0') {
        pLine = pCurrent;
        pLineEnd = strchr(pLine, '\n');
        if (pLineEnd == NULL) {
            pLineEnd = pLine + strlen(pLine);
        }
        
        lineLen = (int)(pLineEnd - pLine);
        if (lineLen == 0) {
            /* Empty line (zero length) - this creates paragraph breaks */
            if (*pLineEnd == '\n') {
                *pWrite = '\n';
                pWrite++;
            }
        } else if (lineLen > 0 && lineLen < (int)(sizeof(line) - 1)) {
            strncpy(line, pLine, lineLen);
            line[lineLen] = '\0';
            
            /* Check if this is an empty or whitespace-only line before trimming */
            int isEmptyLine = 1;
            for (int i = 0; i < lineLen; i++) {
                if (pLine[i] != ' ' && pLine[i] != '\t' && pLine[i] != '\r') {
                    isEmptyLine = 0;
                    break;
                }
            }
            
            fsdoc_trim_whitespace(line);
            
            if (strlen(line) > 0) {
                /* Check if this is an option name */
                const char* pChar = line;
                int isOption = 1;
                
                if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_') {
                    isOption = 0;
                } else {
                    pChar++;
                    while (*pChar != '\0' && isOption) {
                        if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_' && !FSDOC_IS_DIGIT(*pChar)) {
                            isOption = 0;
                        }
                        pChar++;
                    }
                }
                
                if (isOption && strlen(line) > 2) {
                    /* Found first option, break and start table */
                    break;
                } else {
                    /* This is introductory text, copy it */
                    strncpy(pWrite, pLine, pLineEnd - pLine);
                    pWrite += (pLineEnd - pLine);
                    if (*pLineEnd == '\n') {
                        *pWrite = '\n';
                        pWrite++;
                    }
                }
            } else if (isEmptyLine) {
                /* Empty line, copy it to preserve paragraph breaks */
                strncpy(pWrite, pLine, pLineEnd - pLine);
                pWrite += (pLineEnd - pLine);
                if (*pLineEnd == '\n') {
                    *pWrite = '\n';
                    pWrite++;
                }
            }
        }
        
        pCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
    }
    
    /* Add table header */
    if (hasDescriptions) {
        strcpy(pWrite, "\n| Option | Description |\n|:-------|:------------|\n");
    } else {
        strcpy(pWrite, "\n| Option |\n|:-------|\n");
    }
    pWrite += strlen(pWrite);
    
    while (*pCurrent != '\0') {
        pLine = pCurrent;
        pLineEnd = strchr(pLine, '\n');
        if (pLineEnd == NULL) {
            pLineEnd = pLine + strlen(pLine);
        }
        
        lineLen = (int)(pLineEnd - pLine);
        if (lineLen > 0 && lineLen < (int)(sizeof(line) - 1)) {
            strncpy(line, pLine, lineLen);
            line[lineLen] = '\0';
            fsdoc_trim_whitespace(line);
            
            if (strlen(line) > 0) {
                /* Check if this is an option name */
                const char* pChar = line;
                int isOption = 1;
                
                if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_') {
                    isOption = 0;
                } else {
                    pChar++;
                    while (*pChar != '\0' && isOption) {
                        if (!FSDOC_IS_UPPER(*pChar) && *pChar != '_' && !FSDOC_IS_DIGIT(*pChar)) {
                            isOption = 0;
                        }
                        pChar++;
                    }
                }
                
                if (isOption && strlen(line) > 2) {
                    /* This is an option name, start a new table row */
                    if (hasDescriptions) {
                        sprintf(pWrite, "| `%s` | ", line);
                        pWrite += strlen(pWrite);
                        
                        /* Now collect the description from following lines */
                        int first_desc_line = 1;
                        pCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
                        
                        while (*pCurrent != '\0') {
                            const char* pDescLine = pCurrent;
                            const char* pDescEnd = strchr(pDescLine, '\n');
                            if (pDescEnd == NULL) {
                                pDescEnd = pDescLine + strlen(pDescLine);
                            }
                            
                            int descLen = (int)(pDescEnd - pDescLine);
                            if (descLen > 0 && descLen < (int)(sizeof(line) - 1)) {
                                strncpy(line, pDescLine, descLen);
                                line[descLen] = '\0';
                                fsdoc_trim_whitespace(line);
                                
                                if (strlen(line) == 0) {
                                    /* Empty line, might be end of description */
                                    pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                                    break;
                                }
                                
                                /* Check if this is the next option */
                                const char* pCheck = line;
                                int isNextOption = 1;
                                if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_') {
                                    isNextOption = 0;
                                } else {
                                    pCheck++;
                                    while (*pCheck != '\0' && isNextOption) {
                                        if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_' && !FSDOC_IS_DIGIT(*pCheck)) {
                                            isNextOption = 0;
                                        }
                                        pCheck++;
                                    }
                                }
                                
                                if (isNextOption && strlen(line) > 2) {
                                    /* This is the next option, stop collecting description */
                                    break;
                                }
                                
                                /* This is part of the description */
                                if (!first_desc_line) {
                                    strcpy(pWrite, " ");
                                    pWrite++;
                                }
                                strcpy(pWrite, line);
                                pWrite += strlen(line);
                                first_desc_line = 0;
                            }
                            
                            pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                        }
                        
                        /* End the table row */
                        strcpy(pWrite, " |\n");
                        pWrite += 3;
                    } else {
                        /* Single column format - just output the option name */
                        sprintf(pWrite, "| `%s` |\n", line);
                        pWrite += strlen(pWrite);
                        
                        /* Skip any description lines since we're not using them */
                        pCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
                        while (*pCurrent != '\0') {
                            const char* pDescLine = pCurrent;
                            const char* pDescEnd = strchr(pDescLine, '\n');
                            if (pDescEnd == NULL) {
                                pDescEnd = pDescLine + strlen(pDescLine);
                            }
                            
                            int descLen = (int)(pDescEnd - pDescLine);
                            if (descLen > 0 && descLen < (int)(sizeof(line) - 1)) {
                                char tempLine[512];
                                strncpy(tempLine, pDescLine, descLen);
                                tempLine[descLen] = '\0';
                                fsdoc_trim_whitespace(tempLine);
                                
                                if (strlen(tempLine) == 0) {
                                    /* Empty line, might be end of description */
                                    pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                                    break;
                                }
                                
                                /* Check if this is the next option */
                                const char* pCheck = tempLine;
                                int isNextOption = 1;
                                if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_') {
                                    isNextOption = 0;
                                } else {
                                    pCheck++;
                                    while (*pCheck != '\0' && isNextOption) {
                                        if (!FSDOC_IS_UPPER(*pCheck) && *pCheck != '_' && !FSDOC_IS_DIGIT(*pCheck)) {
                                            isNextOption = 0;
                                        }
                                        pCheck++;
                                    }
                                }
                                
                                if (isNextOption && strlen(tempLine) > 2) {
                                    /* This is the next option, stop collecting description */
                                    break;
                                }
                            }
                            
                            pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                        }
                    }
                    
                    continue; /* Don't advance pCurrent again */
                } else {
                    /* Not an option, might be introductory text - add it before the table */
                    /* For now, skip non-option lines that aren't part of descriptions */
                }
            }
        }
        
        pCurrent = (*pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
    }
    
    *pWrite = '\0';
    return pResult;
}

static void fsdoc_trim_whitespace(char* pStr)
{
    char* pStart;
    char* pEnd;
    size_t len;

    if (pStr == NULL) {
        return;
    }

    /* Trim leading whitespace */
    pStart = pStr;
    while (FSDOC_IS_SPACE(*pStart)) {
        pStart++;
    }

    /* Trim trailing whitespace */
    len = strlen(pStart);
    if (len == 0) {
        *pStr = '\0';
        return;
    }

    pEnd = pStart + len - 1;
    while (pEnd > pStart && FSDOC_IS_SPACE(*pEnd)) {
        pEnd--;
    }

    /* Move the trimmed string to the beginning */
    len = pEnd - pStart + 1;
    memmove(pStr, pStart, len);
    pStr[len] = '\0';
}

static void fsdoc_free_function(fsdoc_function* pFunction)
{
    if (pFunction == NULL) {
        return;
    }

    fsdoc_free_params(pFunction->pFirstParam);
    fsdoc_free_see_also(pFunction->pFirstSeeAlso);
    fsdoc_free_examples(pFunction->pFirstExample);
    
    if (pFunction->pComment != NULL) {
        fs_free(pFunction->pComment, NULL);
    }
    
    if (pFunction->pDescription != NULL) {
        fs_free(pFunction->pDescription, NULL);
    }
    
    if (pFunction->pReturnValue != NULL) {
        fs_free(pFunction->pReturnValue, NULL);
    }
    
    fs_free(pFunction, NULL);
}

static void fsdoc_free_params(fsdoc_param* pParam)
{
    while (pParam != NULL) {
        fsdoc_param* pNext = pParam->pNext;
        
        if (pParam->pDescription != NULL) {
            fs_free(pParam->pDescription, NULL);
        }
        
        fs_free(pParam, NULL);
        pParam = pNext;
    }
}

static void fsdoc_free_see_also(fsdoc_see_also* pSeeAlso)
{
    while (pSeeAlso != NULL) {
        fsdoc_see_also* pNext = pSeeAlso->pNext;
        fs_free(pSeeAlso, NULL);
        pSeeAlso = pNext;
    }
}

static void fsdoc_free_examples(fsdoc_example* pExample)
{
    fsdoc_example* pCurrent;
    fsdoc_example* pNext;

    pCurrent = pExample;
    while (pCurrent != NULL) {
        pNext = pCurrent->pNext;
        
        if (pCurrent->pContent != NULL) {
            fs_free(pCurrent->pContent, NULL);
        }
        
        fs_free(pCurrent, NULL);
        pCurrent = pNext;
    }
}
