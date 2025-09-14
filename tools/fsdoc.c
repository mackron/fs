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

typedef struct fsdoc_enum_value
{
    char name[128];
    char value[64];        /* The numeric value or expression */
    char* pDescription;    /* Description from documentation */
    struct fsdoc_enum_value* pNext;
} fsdoc_enum_value;

typedef struct fsdoc_enum
{
    char name[128];
    char* pDescription;    /* Main description from documentation */
    fsdoc_enum_value* pFirstValue;
    struct fsdoc_enum* pNext;
} fsdoc_enum;

typedef struct fsdoc_struct_member
{
    char name[128];
    char type[256];        /* The type of the member */
    char* pDescription;    /* Description from documentation */
    struct fsdoc_struct_member* pNext;
} fsdoc_struct_member;

typedef struct fsdoc_struct
{
    char name[128];
    char* pDescription;    /* Main description from documentation */
    fsdoc_struct_member* pFirstMember;
    int isOpaque;          /* 1 if this is a forward declaration with no definition */
    struct fsdoc_struct* pNext;
} fsdoc_struct;

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

typedef struct fsdoc_full_struct_name
{
    char name[256];
    struct fsdoc_full_struct_name* pNext;
} fsdoc_full_struct_name;

typedef struct fsdoc_context
{
    fsdoc_function* pFirstFunction;
    fsdoc_enum* pFirstEnum;
    fsdoc_struct* pFirstStruct;
    fsdoc_full_struct_name* pFirstFullStructName;
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
static int fsdoc_parse_enum_declaration(const char* pDeclaration, fsdoc_enum* pEnum);
static int fsdoc_parse_struct_declaration(fsdoc_context* pContext, const char* pDeclaration, fsdoc_struct* pStruct);
static void fsdoc_parse_see_also(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_examples(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_return_value(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_description(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_parse_parameters_docs(const char* pComment, fsdoc_function* pFunction);
static void fsdoc_free_function(fsdoc_function* pFunction);
static void fsdoc_free_enum(fsdoc_enum* pEnum);
static void fsdoc_free_enum_values(fsdoc_enum_value* pValue);
static void fsdoc_free_struct(fsdoc_struct* pStruct);
static void fsdoc_free_struct_members(fsdoc_struct_member* pMember);
static void fsdoc_free_full_struct_names(fsdoc_full_struct_name* pName);
static int fsdoc_scan_for_full_struct_definitions(fsdoc_context* pContext);
static int fsdoc_is_struct_fully_defined(fsdoc_context* pContext, const char* pStructName);
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

    /* Free enum list */
    while (pContext->pFirstEnum != NULL) {
        fsdoc_enum* pNext = pContext->pFirstEnum->pNext;
        fsdoc_free_enum(pContext->pFirstEnum);
        pContext->pFirstEnum = pNext;
    }

    /* Free struct list */
    while (pContext->pFirstStruct != NULL) {
        fsdoc_struct* pNext = pContext->pFirstStruct->pNext;
        fsdoc_free_struct(pContext->pFirstStruct);
        pContext->pFirstStruct = pNext;
    }

    /* Free full struct names list */
    fsdoc_free_full_struct_names(pContext->pFirstFullStructName);

    memset(pContext, 0, sizeof(*pContext));
}

static int fsdoc_parse(fsdoc_context* pContext)
{
    char* pCurrent;
    char* pLine;
    char* pNextLine;
    fsdoc_function* pLastFunction;
    fsdoc_enum* pLastEnum;
    fsdoc_struct* pLastStruct;

    if (pContext == NULL || pContext->pFileContent == NULL) {
        return 1;
    }

    /* First pass: scan for all struct definitions with full implementations */
    fsdoc_scan_for_full_struct_definitions(pContext);

    pLastFunction = NULL;
    pLastEnum = NULL;
    pLastStruct = NULL;
    pCurrent = pContext->pFileContent;

    while (*pCurrent != '\0') {
        /* Find the next FS_API function, typedef enum, typedef struct, or standalone struct */
        char* pFunctionLine = strstr(pCurrent, "FS_API");
        char* pEnumLine = strstr(pCurrent, "typedef enum");
        char* pStructLine = strstr(pCurrent, "typedef struct");
        char* pPlainStructLine = strstr(pCurrent, "struct ");
        
        /* For plain struct, make sure it's at start of line and followed by name { */
        if (pPlainStructLine != NULL) {
            /* Check if it's at start of line (after newline or at beginning) */
            if (pPlainStructLine != pContext->pFileContent && *(pPlainStructLine - 1) != '\n') {
                /* Not at start of line, skip this one */
                char* pNext = strstr(pPlainStructLine + 1, "struct ");
                pPlainStructLine = pNext;
            } else {
                /* Check if it has a name and opening brace (not a typedef struct) */
                char* pNameStart = pPlainStructLine + 7; /* strlen("struct ") */
                while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
                    pNameStart++;
                }
                if (*pNameStart != '\0' && !FSDOC_IS_SPACE(*pNameStart)) {
                    char* pBrace = strchr(pNameStart, '{');
                    if (pBrace == NULL) {
                        /* No brace found, skip this one */
                        pPlainStructLine = NULL;
                    }
                } else {
                    /* No name found, skip this one */
                    pPlainStructLine = NULL;
                }
            }
        }
        
        /* Determine which comes first */
        char* pNext = NULL;
        int type = 0; /* 0 = none, 1 = function, 2 = enum, 3 = struct, 4 = plain struct */
        
        if (pFunctionLine != NULL) {
            pNext = pFunctionLine;
            type = 1;
        }
        
        if (pEnumLine != NULL && (pNext == NULL || pEnumLine < pNext)) {
            pNext = pEnumLine;
            type = 2;
        }
        
        if (pStructLine != NULL && (pNext == NULL || pStructLine < pNext)) {
            pNext = pStructLine;
            type = 3;
        }
        
        if (pPlainStructLine != NULL && (pNext == NULL || pPlainStructLine < pNext)) {
            pNext = pPlainStructLine;
            type = 4;
        }
        
        if (pNext == NULL) {
            break;
        }
        
        pLine = pNext;
        
        /* Make sure it's at the beginning of a line (except for enums/structs which might be indented) */
        if (type == 1 && pLine != pContext->pFileContent && *(pLine - 1) != '\n') {
            pCurrent = pLine + 6;
            continue;
        }
        
        if (type == 1) {
            /* Handle FS_API function */
            pNextLine = strchr(pLine, ';');
            if (pNextLine == NULL) {
                pCurrent = pLine + 6;
                continue;
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
            
        } else if (type == 2) {
            /* Handle enum - find the end of the enum (closing brace and typedef) */
            char* pEnumEnd = pLine;
            int braceCount = 0;
            int foundOpenBrace = 0;
            
            while (*pEnumEnd != '\0') {
                if (*pEnumEnd == '{') {
                    foundOpenBrace = 1;
                    braceCount++;
                } else if (*pEnumEnd == '}') {
                    braceCount--;
                    if (foundOpenBrace && braceCount == 0) {
                        /* Find the semicolon after the typedef name */
                        pEnumEnd++;
                        while (*pEnumEnd != '\0' && *pEnumEnd != ';') {
                            pEnumEnd++;
                        }
                        if (*pEnumEnd == ';') {
                            pEnumEnd++;
                        }
                        break;
                    }
                }
                pEnumEnd++;
            }
            
            if (braceCount != 0) {
                pCurrent = pLine + 12;
                continue;
            }
            
            /* Extract the enum declaration */
            size_t declarationLen = pEnumEnd - pLine;
            char* pDeclaration = fs_malloc(declarationLen + 1, NULL);
            if (pDeclaration == NULL) {
                return 1;
            }
            
            strncpy(pDeclaration, pLine, declarationLen);
            pDeclaration[declarationLen] = '\0';
            
            /* Create a new enum */
            fsdoc_enum* pEnum = fs_malloc(sizeof(fsdoc_enum), NULL);
            if (pEnum == NULL) {
                fs_free(pDeclaration, NULL);
                return 1;
            }
            
            memset(pEnum, 0, sizeof(*pEnum));
            
            /* Parse the enum declaration */
            if (fsdoc_parse_enum_declaration(pDeclaration, pEnum) == 0) {
                /* Add to the list */
                if (pLastEnum == NULL) {
                    pContext->pFirstEnum = pEnum;
                } else {
                    pLastEnum->pNext = pEnum;
                }
                pLastEnum = pEnum;
            } else {
                fsdoc_free_enum(pEnum);
            }
            
            fs_free(pDeclaration, NULL);
            pCurrent = pEnumEnd;
            
        } else if (type == 3) {
            /* Handle struct - find the end of the struct */
            char* pStructEnd = pLine;
            int braceCount = 0;
            int foundOpenBrace = 0;
            
            /* Check if this is a forward declaration (no brace) */
            char* pSemicolon = strchr(pLine, ';');
            char* pOpenBrace = strchr(pLine, '{');
            
            if (pSemicolon != NULL && (pOpenBrace == NULL || pSemicolon < pOpenBrace)) {
                /* Forward declaration - just go to semicolon */
                pStructEnd = pSemicolon + 1;
            } else {
                /* Full struct definition - find matching closing brace */
                while (*pStructEnd != '\0') {
                    if (*pStructEnd == '{') {
                        foundOpenBrace = 1;
                        braceCount++;
                    } else if (*pStructEnd == '}') {
                        braceCount--;
                        if (foundOpenBrace && braceCount == 0) {
                            /* Find the semicolon after the typedef name */
                            pStructEnd++;
                            while (*pStructEnd != '\0' && *pStructEnd != ';') {
                                pStructEnd++;
                            }
                            if (*pStructEnd == ';') {
                                pStructEnd++;
                            }
                            break;
                        }
                    }
                    pStructEnd++;
                }
                
                if (foundOpenBrace && braceCount != 0) {
                    pCurrent = pLine + 14; /* strlen("typedef struct") */
                    continue;
                }
            }
            
            /* Extract the struct declaration */
            size_t declarationLen = pStructEnd - pLine;
            char* pDeclaration = fs_malloc(declarationLen + 1, NULL);
            if (pDeclaration == NULL) {
                return 1;
            }
            
            strncpy(pDeclaration, pLine, declarationLen);
            pDeclaration[declarationLen] = '\0';
            
            /* Check if this struct is inside a comment block */
            int inCommentBlock = 0;
            char* pCommentCheck = pLine - 1;
            while (pCommentCheck >= pContext->pFileContent) {
                if (pCommentCheck[0] == '/' && pCommentCheck + 1 < pLine && pCommentCheck[1] == '*') {
                    inCommentBlock = 1;
                    break;
                }
                if (pCommentCheck[0] == '*' && pCommentCheck + 1 < pLine && pCommentCheck[1] == '/') {
                    break;
                }
                if (*pCommentCheck == '\n') {
                    /* Look for comment markers at start of line */
                    char* pLineStart = pCommentCheck + 1;
                    while (pLineStart < pLine && (*pLineStart == ' ' || *pLineStart == '\t')) {
                        pLineStart++;
                    }
                    if (pLineStart < pLine && *pLineStart == '*') {
                        inCommentBlock = 1;
                        break;
                    }
                    if (pLineStart < pLine && *pLineStart != '*' && *pLineStart != '\n') {
                        break;
                    }
                }
                pCommentCheck--;
            }
            
            if (!inCommentBlock) {
                /* Create a new struct */
                fsdoc_struct* pStruct = fs_malloc(sizeof(fsdoc_struct), NULL);
                if (pStruct == NULL) {
                    fs_free(pDeclaration, NULL);
                    return 1;
                }
                
                memset(pStruct, 0, sizeof(*pStruct));
                
                /* Parse the struct declaration */
                if (fsdoc_parse_struct_declaration(pContext, pDeclaration, pStruct) == 0) {
                    /* Add to the list */
                    if (pLastStruct == NULL) {
                        pContext->pFirstStruct = pStruct;
                    } else {
                        pLastStruct->pNext = pStruct;
                    }
                    pLastStruct = pStruct;
                } else {
                    fsdoc_free_struct(pStruct);
                }
            }
            
            fs_free(pDeclaration, NULL);
            pCurrent = pStructEnd;
            
        } else if (type == 4) {
            /* Handle plain struct definition - struct name { ... }; */
            char* pStructEnd = pLine;
            int braceCount = 0;
            int foundOpenBrace = 0;
            
            /* Find the matching closing brace */
            while (*pStructEnd != '\0') {
                if (*pStructEnd == '{') {
                    foundOpenBrace = 1;
                    braceCount++;
                } else if (*pStructEnd == '}') {
                    braceCount--;
                    if (foundOpenBrace && braceCount == 0) {
                        /* Find the semicolon after the closing brace */
                        pStructEnd++;
                        while (*pStructEnd != '\0' && *pStructEnd != ';') {
                            pStructEnd++;
                        }
                        if (*pStructEnd == ';') {
                            pStructEnd++;
                        }
                        break;
                    }
                }
                pStructEnd++;
            }
            
            if (!foundOpenBrace || braceCount != 0) {
                pCurrent = pLine + 7; /* strlen("struct ") */
                continue;
            }
            
            /* Extract the struct declaration */
            size_t declarationLen = pStructEnd - pLine;
            char* pDeclaration = fs_malloc(declarationLen + 1, NULL);
            if (pDeclaration == NULL) {
                return 1;
            }
            
            strncpy(pDeclaration, pLine, declarationLen);
            pDeclaration[declarationLen] = '\0';
            
            /* Check if this struct is inside a comment block */
            int inCommentBlock = 0;
            char* pCommentCheck = pLine - 1;
            while (pCommentCheck >= pContext->pFileContent) {
                if (pCommentCheck[0] == '/' && pCommentCheck + 1 < pLine && pCommentCheck[1] == '*') {
                    inCommentBlock = 1;
                    break;
                }
                if (pCommentCheck[0] == '*' && pCommentCheck + 1 < pLine && pCommentCheck[1] == '/') {
                    break;
                }
                if (*pCommentCheck == '\n') {
                    /* Look for comment markers at start of line */
                    char* pLineStart = pCommentCheck + 1;
                    while (pLineStart < pLine && FSDOC_IS_SPACE(*pLineStart)) {
                        pLineStart++;
                    }
                    if (pLineStart < pLine && *pLineStart == '*') {
                        inCommentBlock = 1;
                        break;
                    }
                    if (pLineStart < pLine && pLineStart + 1 < pLine && pLineStart[0] == '/' && pLineStart[1] == '/') {
                        break; /* Single line comment, not a block */
                    }
                    break;
                }
                pCommentCheck--;
            }
            
            if (!inCommentBlock) {
                /* Extract struct name first */
                char* pNameStart = pLine + 7; /* strlen("struct ") */
                while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
                    pNameStart++;
                }
                char* pNameEnd = pNameStart;
                while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != '{') {
                    pNameEnd++;
                }
                
                size_t nameLen = pNameEnd - pNameStart;
                if (nameLen > 0 && nameLen < 256) {
                    char structName[256];
                    strncpy(structName, pNameStart, nameLen);
                    structName[nameLen] = '\0';
                    
                    /* Check if we already have a struct with this name */
                    fsdoc_struct* pExistingStruct = NULL;
                    fsdoc_struct* pCurrent;
                    for (pCurrent = pContext->pFirstStruct; pCurrent != NULL; pCurrent = pCurrent->pNext) {
                        if (strcmp(pCurrent->name, structName) == 0) {
                            pExistingStruct = pCurrent;
                            break;
                        }
                    }
                    
                    if (pExistingStruct != NULL && pExistingStruct->pFirstMember == NULL) {
                        /* We have an existing struct (likely from forward declaration) with no members.
                           Parse the members from this full definition and add them to the existing struct. */
                        size_t convertedLen = declarationLen + 50;
                        char* pConvertedDeclaration = fs_malloc(convertedLen, NULL);
                        if (pConvertedDeclaration != NULL) {
                            /* Build converted declaration: typedef struct name { ... } name; */
                            strcpy(pConvertedDeclaration, "typedef ");
                            strncat(pConvertedDeclaration, pLine, pStructEnd - pLine);
                            strcat(pConvertedDeclaration, " ");
                            strcat(pConvertedDeclaration, structName);
                            strcat(pConvertedDeclaration, ";");
                            
                            /* Parse just the members into a temporary struct */
                            fsdoc_struct tempStruct;
                            memset(&tempStruct, 0, sizeof(tempStruct));
                            
                            if (fsdoc_parse_struct_declaration(pContext, pConvertedDeclaration, &tempStruct) == 0) {
                                /* Move the members from temp struct to existing struct */
                                pExistingStruct->pFirstMember = tempStruct.pFirstMember;
                                tempStruct.pFirstMember = NULL; /* Prevent double-free */
                            }
                            
                            fs_free(pConvertedDeclaration, NULL);
                        }
                    }
                    /* If no existing struct found, or existing struct already has members, skip this one
                       since it would be a duplicate or conflict */
                }
            }
            
            fs_free(pDeclaration, NULL);
            pCurrent = pStructEnd;
        }
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
                            /* Special case for variadic parameters: match "..." documented param with empty name but "..." type */
                            if ((strcmp(pParam->name, paramName) == 0) ||
                                (strcmp(paramName, "...") == 0 && strcmp(pParam->type, "...") == 0 && strlen(pParam->name) == 0)) {
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

static int fsdoc_parse_enum_declaration(const char* pDeclaration, fsdoc_enum* pEnum)
{
    const char* pEnumKeyword;
    const char* pNameStart;
    const char* pNameEnd;
    const char* pBodyStart;
    const char* pBodyEnd;
    size_t nameLen;

    if (pDeclaration == NULL || pEnum == NULL) {
        return 1;
    }

    /* Find "typedef enum" */
    pEnumKeyword = strstr(pDeclaration, "typedef enum");
    if (pEnumKeyword == NULL) {
        return 1;
    }

    /* Skip past "typedef enum" and any whitespace */
    pNameStart = pEnumKeyword + 12; /* 12 = strlen("typedef enum") */
    while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
        pNameStart++;
    }

    /* Check if there's a name before the opening brace */
    if (*pNameStart == '{') {
        /* Anonymous enum - name comes after the closing brace */
        pBodyStart = pNameStart;
    } else {
        /* Named enum - find the opening brace */
        pNameEnd = pNameStart;
        while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != '{') {
            pNameEnd++;
        }
        
        /* For now, skip the tag name and look for the typedef name after the brace */
        pBodyStart = strchr(pNameEnd, '{');
    }
    
    if (pBodyStart == NULL) {
        return 1;
    }
    
    /* Find the closing brace */
    pBodyEnd = strrchr(pDeclaration, '}');
    if (pBodyEnd == NULL || pBodyEnd <= pBodyStart) {
        return 1;
    }
    
    /* Find the typedef name after the closing brace */
    pNameStart = pBodyEnd + 1;
    while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
        pNameStart++;
    }
    
    pNameEnd = pNameStart;
    while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != ';') {
        pNameEnd++;
    }
    
    nameLen = pNameEnd - pNameStart;
    if (nameLen == 0 || nameLen >= sizeof(pEnum->name)) {
        return 1;
    }
    
    strncpy(pEnum->name, pNameStart, nameLen);
    pEnum->name[nameLen] = '\0';

    /* Parse enum values from the body */
    const char* pCurrent = pBodyStart + 1; /* Skip opening brace */
    fsdoc_enum_value* pLastValue = NULL;
    
    while (pCurrent < pBodyEnd) {
        const char* pLineStart = pCurrent;
        const char* pLineEnd = strchr(pLineStart, '\n');
        if (pLineEnd == NULL || pLineEnd > pBodyEnd) {
            pLineEnd = pBodyEnd;
        }
        
        /* Extract the line */
        size_t lineLen = pLineEnd - pLineStart;
        if (lineLen > 0 && lineLen < 512) {
            char line[512];
            strncpy(line, pLineStart, lineLen);
            line[lineLen] = '\0';
            
            /* Trim whitespace and remove comma */
            fsdoc_trim_whitespace(line);
            if (strlen(line) > 0 && line[strlen(line) - 1] == ',') {
                line[strlen(line) - 1] = '\0';
                fsdoc_trim_whitespace(line);
            }
            
            /* Skip empty lines and comments */
            if (strlen(line) > 0 && line[0] != '/' && line[0] != '*') {
                fsdoc_enum_value* pValue = fs_malloc(sizeof(fsdoc_enum_value), NULL);
                if (pValue != NULL) {
                    memset(pValue, 0, sizeof(*pValue));
                    
                    /* Look for '=' to separate name from value */
                    char* pEquals = strchr(line, '=');
                    if (pEquals != NULL) {
                        /* Split name and value */
                        *pEquals = '\0';
                        fsdoc_trim_whitespace(line);
                        
                        char* valueStr = pEquals + 1;
                        fsdoc_trim_whitespace(valueStr);
                        
                        /* Remove comments from value */
                        char* commentStart = strstr(valueStr, "/*");
                        if (commentStart != NULL) {
                            *commentStart = '\0';
                            fsdoc_trim_whitespace(valueStr);
                        }
                        
                        /* Remove trailing comma from value */
                        if (strlen(valueStr) > 0 && valueStr[strlen(valueStr) - 1] == ',') {
                            valueStr[strlen(valueStr) - 1] = '\0';
                            fsdoc_trim_whitespace(valueStr);
                        }
                        
                        strncpy(pValue->name, line, sizeof(pValue->name) - 1);
                        pValue->name[sizeof(pValue->name) - 1] = '\0';
                        
                        strncpy(pValue->value, valueStr, sizeof(pValue->value) - 1);
                        pValue->value[sizeof(pValue->value) - 1] = '\0';
                    } else {
                        /* Just the name */
                        strncpy(pValue->name, line, sizeof(pValue->name) - 1);
                        pValue->name[sizeof(pValue->name) - 1] = '\0';
                        pValue->value[0] = '\0';
                    }
                    
                    /* Add to the list */
                    if (pLastValue == NULL) {
                        pEnum->pFirstValue = pValue;
                    } else {
                        pLastValue->pNext = pValue;
                    }
                    pLastValue = pValue;
                }
            }
        }
        
        pCurrent = (pLineEnd < pBodyEnd && *pLineEnd == '\n') ? pLineEnd + 1 : pBodyEnd;
    }

    return 0;
}

static int fsdoc_parse_struct_declaration(fsdoc_context* pContext, const char* pDeclaration, fsdoc_struct* pStruct)
{
    const char* pStructKeyword;
    const char* pNameStart;
    const char* pNameEnd;
    const char* pBodyStart;
    const char* pBodyEnd;
    size_t nameLen;

    if (pDeclaration == NULL || pStruct == NULL) {
        return 1;
    }

    /* Find "typedef struct" */
    pStructKeyword = strstr(pDeclaration, "typedef struct");
    if (pStructKeyword == NULL) {
        return 1;
    }

    /* Skip past "typedef struct" and any whitespace */
    pNameStart = pStructKeyword + 14; /* 14 = strlen("typedef struct") */
    while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
        pNameStart++;
    }

    /* Check if this is a forward declaration (just typedef struct name;) */
    const char* pSemicolon = strchr(pNameStart, ';');
    const char* pOpenBrace = strchr(pNameStart, '{');
    
    if (pSemicolon != NULL && (pOpenBrace == NULL || pSemicolon < pOpenBrace)) {
        /* Forward declaration - extract name before semicolon */
        pNameEnd = pNameStart;
        while (pNameEnd < pSemicolon && !FSDOC_IS_SPACE(*pNameEnd)) {
            pNameEnd++;
        }
        
        nameLen = pNameEnd - pNameStart;
        if (nameLen >= sizeof(pStruct->name)) {
            return 1;
        }
        
        strncpy(pStruct->name, pNameStart, nameLen);
        pStruct->name[nameLen] = '\0';
        
        /* Check if this struct has a full definition elsewhere in the file */
        pStruct->isOpaque = !fsdoc_is_struct_fully_defined(pContext, pStruct->name);
        pStruct->pFirstMember = NULL;
        
        return 0;
    }

    /* Full struct definition */
    pStruct->isOpaque = 0;
    
    /* Find the opening brace */
    pBodyStart = strchr(pNameStart, '{');
    if (pBodyStart == NULL) {
        return 1;
    }
    
    /* Find the matching closing brace */
    pBodyEnd = pBodyStart + 1;
    int braceCount = 1;
    while (*pBodyEnd != '\0' && braceCount > 0) {
        if (*pBodyEnd == '{') {
            braceCount++;
        } else if (*pBodyEnd == '}') {
            braceCount--;
        }
        pBodyEnd++;
    }
    
    if (braceCount != 0) {
        return 1;
    }
    pBodyEnd--; /* Point to the closing brace */
    
    /* Find the struct name after the closing brace */
    const char* pAfterBrace = pBodyEnd + 1;
    while (*pAfterBrace != '\0' && FSDOC_IS_SPACE(*pAfterBrace)) {
        pAfterBrace++;
    }
    
    pNameStart = pAfterBrace;
    pNameEnd = pNameStart;
    while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != ';') {
        pNameEnd++;
    }
    
    nameLen = pNameEnd - pNameStart;
    if (nameLen >= sizeof(pStruct->name)) {
        return 1;
    }
    
    strncpy(pStruct->name, pNameStart, nameLen);
    pStruct->name[nameLen] = '\0';
    
    /* Parse struct members with nested struct detection */
    const char* pCurrent = pBodyStart + 1; /* Skip opening brace */
    fsdoc_struct_member* pLastMember = NULL;
    
    /* Simple state tracking for nested structs */
    const char* pNestedStructStart = NULL;
    int isInNestedStruct = 0;
    int nestedBraceCount = 0;
    
    while (pCurrent < pBodyEnd) {
        const char* pLineStart = pCurrent;
        const char* pLineEnd = strchr(pLineStart, '\n');
        if (pLineEnd == NULL || pLineEnd > pBodyEnd) {
            pLineEnd = pBodyEnd;
        }
        
        /* Extract the line */
        size_t lineLen = pLineEnd - pLineStart;
        if (lineLen > 0 && lineLen < 512) {
            char line[512];
            strncpy(line, pLineStart, lineLen);
            line[lineLen] = '\0';
            
            /* Trim whitespace and remove inline comments first */
            fsdoc_trim_whitespace(line);
            
            /* Remove inline comments */
            char* pCommentStart = strstr(line, "/*");
            if (pCommentStart != NULL) {
                *pCommentStart = '\0';
                fsdoc_trim_whitespace(line);
            }
            pCommentStart = strstr(line, "//");
            if (pCommentStart != NULL) {
                *pCommentStart = '\0';
                fsdoc_trim_whitespace(line);
            }
            pCommentStart = strstr(line, "//");
            if (pCommentStart != NULL) {
                *pCommentStart = '\0';
                fsdoc_trim_whitespace(line);
            }
            
            /* Then remove semicolon */
            if (strlen(line) > 0 && line[strlen(line) - 1] == ';') {
                line[strlen(line) - 1] = '\0';
                fsdoc_trim_whitespace(line);
            }
            
            /* Handle nested struct state machine */
            if (!isInNestedStruct && strcmp(line, "struct") == 0) {
                /* Start of potential nested struct */
                isInNestedStruct = 1;
                pNestedStructStart = pLineStart;
                nestedBraceCount = 0;
            } else if (isInNestedStruct && strcmp(line, "{") == 0) {
                /* Opening brace of nested struct */
                nestedBraceCount = 1;
            } else if (isInNestedStruct && nestedBraceCount > 0) {
                /* Inside nested struct - check for closing brace with member name */
                if (line[0] == '}' && strlen(line) > 1) {
                    /* Found "} memberName" pattern */
                    char* pAfterBrace = line + 1;
                    while (*pAfterBrace != '\0' && FSDOC_IS_SPACE(*pAfterBrace)) {
                        pAfterBrace++;
                    }
                    
                    if (strlen(pAfterBrace) > 0) {
                        /* Create nested struct member */
                        fsdoc_struct_member* pMember = fs_malloc(sizeof(fsdoc_struct_member), NULL);
                        if (pMember != NULL) {
                            memset(pMember, 0, sizeof(*pMember));
                            
                            /* Set member name */
                            strncpy(pMember->name, pAfterBrace, sizeof(pMember->name) - 1);
                            pMember->name[sizeof(pMember->name) - 1] = '\0';
                            
                            /* Build nested struct type */
                            strcpy(pMember->type, "struct {");
                            
                            /* Extract content - skip "struct" and "{" lines, get content before "}" line */
                            const char* pContentStart = pNestedStructStart;
                            
                            /* Skip to line after "struct" */
                            const char* pStructLine = strchr(pContentStart, '\n');
                            if (pStructLine != NULL) {
                                pStructLine++; /* Skip to line after "struct" */
                                /* Skip to line after "{" */
                                const char* pBraceLine = strchr(pStructLine, '\n');
                                if (pBraceLine != NULL) {
                                    pContentStart = pBraceLine + 1; /* Start after "{" line */
                                }
                            }
                            
                            /* Extract nested content up to current line */
                            const char* pContentScan = pContentStart;
                            while (pContentScan < pLineStart) {
                                const char* pScanLineEnd = strchr(pContentScan, '\n');
                                if (pScanLineEnd == NULL || pScanLineEnd > pLineStart) {
                                    pScanLineEnd = pLineStart;
                                }
                                
                                size_t scanLineLen = pScanLineEnd - pContentScan;
                                if (scanLineLen > 0 && scanLineLen < 256) {
                                    char scanLine[256];
                                    strncpy(scanLine, pContentScan, scanLineLen);
                                    scanLine[scanLineLen] = '\0';
                                    fsdoc_trim_whitespace(scanLine);
                                    
                                    /* Remove comments and semicolon */
                                    char* pComment = strstr(scanLine, "/*");
                                    if (pComment != NULL) {
                                        *pComment = '\0';
                                        fsdoc_trim_whitespace(scanLine);
                                    }
                                    pComment = strstr(scanLine, "//");
                                    if (pComment != NULL) {
                                        *pComment = '\0';
                                        fsdoc_trim_whitespace(scanLine);
                                    }
                                    if (strlen(scanLine) > 0 && scanLine[strlen(scanLine) - 1] == ';') {
                                        scanLine[strlen(scanLine) - 1] = '\0';
                                        fsdoc_trim_whitespace(scanLine);
                                    }
                                    
                                    if (strlen(scanLine) > 0 && scanLine[0] != '/' && scanLine[0] != '*') {
                                        strcat(pMember->type, "\n        ");
                                        strcat(pMember->type, scanLine);
                                        strcat(pMember->type, ";");
                                    }
                                }
                                
                                pContentScan = (pScanLineEnd < pLineStart && *pScanLineEnd == '\n') ? pScanLineEnd + 1 : pScanLineEnd;
                            }
                            
                            strcat(pMember->type, "\n    }");
                            
                            pMember->pDescription = fs_malloc(1, NULL);
                            if (pMember->pDescription != NULL) {
                                pMember->pDescription[0] = '\0';
                            }
                            
                            /* Add to list */
                            if (pLastMember == NULL) {
                                pStruct->pFirstMember = pMember;
                            } else {
                                pLastMember->pNext = pMember;
                            }
                            pLastMember = pMember;
                        }
                    }
                    
                    /* Reset state */
                    isInNestedStruct = 0;
                    nestedBraceCount = 0;
                    pNestedStructStart = NULL;
                    
                    /* Move to next line since we processed this closing brace line */
                    pCurrent = (pLineEnd < pBodyEnd && *pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
                    continue;
                }
            } else if (isInNestedStruct && nestedBraceCount == 0) {
                /* We saw "struct" but next line wasn't "{" - not a nested struct */
                isInNestedStruct = 0;
                pNestedStructStart = NULL;
                /* Continue processing this line as normal since it's not part of nested struct */
            }
            
            /* Skip processing if we're inside a nested struct */
            if (isInNestedStruct) {
                /* Move to next line */
                pCurrent = (pLineEnd < pBodyEnd && *pLineEnd == '\n') ? pLineEnd + 1 : pLineEnd;
                continue;
            }
            
            /* Skip empty lines, comments, braces, and lines that look like member names without types */
            if (strlen(line) > 0 && line[0] != '/' && line[0] != '*' && 
                strcmp(line, "{") != 0 && strcmp(line, "}") != 0 && 
                strstr(line, "struct {") == NULL && strcmp(line, "struct") != 0 &&
                /* Skip standalone identifiers that look like member names from nested structs */
                !(strchr(line, ' ') == NULL && 
                  (strcmp(line, "readonly") == 0 || strcmp(line, "write") == 0 || 
                   ((line[0] >= 'a' && line[0] <= 'z') || (line[0] >= 'A' && line[0] <= 'Z'))))) {
                /* Check for nested struct: struct { ... } memberName; */
                char* pStructKeyword = strstr(line, "struct");
                char* pOpenBrace = NULL;
                
                if (pStructKeyword != NULL) {
                    /* Look for opening brace after struct keyword */
                    char* pAfterStruct = pStructKeyword + 6; /* strlen("struct") */
                    while (*pAfterStruct != '\0' && FSDOC_IS_SPACE(*pAfterStruct)) {
                        pAfterStruct++;
                    }
                    if (*pAfterStruct == '{') {
                        pOpenBrace = pAfterStruct;
                    }
                }
                
                if (pOpenBrace != NULL) {
                    /* This is a nested struct - find the complete nested structure */
                    const char* pNestedCurrent = pOpenBrace + 1;
                    int braceCount = 1;
                    
                    /* Find the matching closing brace by scanning forward in the body */
                    while (pNestedCurrent < pBodyEnd && braceCount > 0) {
                        if (*pNestedCurrent == '{') {
                            braceCount++;
                        } else if (*pNestedCurrent == '}') {
                            braceCount--;
                        }
                        pNestedCurrent++;
                    }
                    
                    if (braceCount == 0) {
                        /* Found the complete nested struct - extract member name after closing brace */
                        const char* pAfterBrace = pNestedCurrent;
                        while (pAfterBrace < pBodyEnd && FSDOC_IS_SPACE(*pAfterBrace)) {
                            pAfterBrace++;
                        }
                        
                        const char* pMemberNameStart = pAfterBrace;
                        const char* pMemberNameEnd = pMemberNameStart;
                        while (pMemberNameEnd < pBodyEnd && !FSDOC_IS_SPACE(*pMemberNameEnd) && *pMemberNameEnd != ';' && *pMemberNameEnd != '\n') {
                            pMemberNameEnd++;
                        }
                        
                        if (pMemberNameEnd > pMemberNameStart) {
                            /* Create a nested struct member */
                            fsdoc_struct_member* pMember = fs_malloc(sizeof(fsdoc_struct_member), NULL);
                            if (pMember != NULL) {
                                memset(pMember, 0, sizeof(*pMember));
                                
                                /* Set member name */
                                size_t nameLen = pMemberNameEnd - pMemberNameStart;
                                if (nameLen < sizeof(pMember->name)) {
                                    strncpy(pMember->name, pMemberNameStart, nameLen);
                                    pMember->name[nameLen] = '\0';
                                }
                                
                                /* Extract and format nested struct members */
                                const char* pContentStart = pOpenBrace + 1;
                                const char* pContentEnd = pNestedCurrent - 1; /* Exclude closing brace */
                                
                                /* Copy the content and format it properly */
                                size_t maxTypeLen = sizeof(pMember->type) - 50; /* Leave space for formatting */
                                size_t contentLen = pContentEnd - pContentStart;
                                
                                strcpy(pMember->type, "struct {");
                                
                                if (contentLen > 0 && contentLen < maxTypeLen) {
                                    char content[1024];
                                    strncpy(content, pContentStart, contentLen);
                                    content[contentLen] = '\0';
                                    
                                    /* Clean up the content and format it */
                                    char* line_tok = strtok(content, "\n");
                                    while (line_tok != NULL) {
                                        /* Trim and clean each line */
                                        fsdoc_trim_whitespace(line_tok);
                                        if (strlen(line_tok) > 0 && line_tok[0] != '/' && line_tok[0] != '*') {
                                            /* Remove trailing semicolon if present */
                                            if (strlen(line_tok) > 0 && line_tok[strlen(line_tok) - 1] == ';') {
                                                line_tok[strlen(line_tok) - 1] = '\0';
                                            }
                                            /* Remove inline comments */
                                            char* pComment = strstr(line_tok, "//");
                                            if (pComment != NULL) {
                                                *pComment = '\0';
                                                fsdoc_trim_whitespace(line_tok);
                                            }
                                            pComment = strstr(line_tok, "/*");
                                            if (pComment != NULL) {
                                                *pComment = '\0';
                                                fsdoc_trim_whitespace(line_tok);
                                            }
                                            
                                            if (strlen(line_tok) > 0) {
                                                strcat(pMember->type, "\n        ");
                                                strcat(pMember->type, line_tok);
                                                strcat(pMember->type, ";");
                                            }
                                        }
                                        line_tok = strtok(NULL, "\n");
                                    }
                                }
                                
                                strcat(pMember->type, "\n    }");
                                
                                pMember->pDescription = fs_malloc(1, NULL);
                                if (pMember->pDescription != NULL) {
                                    pMember->pDescription[0] = '\0';
                                }
                                
                                /* Add to the list */
                                if (pLastMember == NULL) {
                                    pStruct->pFirstMember = pMember;
                                } else {
                                    pLastMember->pNext = pMember;
                                }
                                pLastMember = pMember;
                            }
                        }
                        
                        /* Skip to after the nested struct */
                        pCurrent = pNestedCurrent;
                        while (pCurrent < pBodyEnd && *pCurrent != '\n') {
                            pCurrent++;
                        }
                        if (pCurrent < pBodyEnd && *pCurrent == '\n') {
                            pCurrent++;
                        }
                        continue;
                    }
                }
                
                /* Regular member parsing */
                fsdoc_struct_member* pMember = fs_malloc(sizeof(fsdoc_struct_member), NULL);
                if (pMember != NULL) {
                    memset(pMember, 0, sizeof(*pMember));
                    
                    /* Parse type and name - handle function pointers and complex types */
                    char* pFuncPtrStart = strstr(line, "(*");
                    
                    if (pFuncPtrStart != NULL) {
                        /* Function pointer: void* (*onMalloc)(size_t sz, void* pUserData) */
                        char* pNameStart = pFuncPtrStart + 2;
                        char* pNameEnd = strchr(pNameStart, ')');
                        
                        if (pNameEnd != NULL) {
                            /* Extract function name */
                            size_t nameLen = pNameEnd - pNameStart;
                            if (nameLen < sizeof(pMember->name)) {
                                strncpy(pMember->name, pNameStart, nameLen);
                                pMember->name[nameLen] = '\0';
                                fsdoc_trim_whitespace(pMember->name);
                                
                                /* Build type by replacing name with placeholder */
                                char typeBuffer[512];
                                size_t prefixLen = pNameStart - line;
                                strncpy(typeBuffer, line, prefixLen);
                                typeBuffer[prefixLen] = '\0';
                                strcat(typeBuffer, pNameEnd);
                                
                                strncpy(pMember->type, typeBuffer, sizeof(pMember->type) - 1);
                                pMember->type[sizeof(pMember->type) - 1] = '\0';
                                fsdoc_trim_whitespace(pMember->type);
                            }
                        }
                    } else {
                        /* Regular variable - find the last space that's not inside brackets/parens */
                        int bracketDepth = 0;
                        int parenDepth = 0;
                        int lastSpacePos = -1;
                        int i;
                        
                        for (i = strlen(line) - 1; i >= 0; i--) {
                            if (line[i] == ']') bracketDepth++;
                            else if (line[i] == '[') bracketDepth--;
                            else if (line[i] == ')') parenDepth++;
                            else if (line[i] == '(') parenDepth--;
                            else if (line[i] == ' ' && bracketDepth == 0 && parenDepth == 0) {
                                lastSpacePos = i;
                                break;
                            }
                        }
                        
                        if (lastSpacePos >= 0) {
                            line[lastSpacePos] = '\0';
                            fsdoc_trim_whitespace(line);
                            fsdoc_trim_whitespace(line + lastSpacePos + 1);
                            
                            strncpy(pMember->type, line, sizeof(pMember->type) - 1);
                            pMember->type[sizeof(pMember->type) - 1] = '\0';
                            
                            strncpy(pMember->name, line + lastSpacePos + 1, sizeof(pMember->name) - 1);
                            pMember->name[sizeof(pMember->name) - 1] = '\0';
                        } else {
                            /* No space found - treat entire line as name with unknown type */
                            strncpy(pMember->type, "unknown", sizeof(pMember->type) - 1);
                            pMember->type[sizeof(pMember->type) - 1] = '\0';
                            
                            strncpy(pMember->name, line, sizeof(pMember->name) - 1);
                            pMember->name[sizeof(pMember->name) - 1] = '\0';
                        }
                    }
                    
                    pMember->pDescription = fs_malloc(1, NULL);
                    if (pMember->pDescription != NULL) {
                        pMember->pDescription[0] = '\0';
                    }
                    
                    /* Add to the list */
                    if (pLastMember == NULL) {
                        pStruct->pFirstMember = pMember;
                    } else {
                        pLastMember->pNext = pMember;
                    }
                    pLastMember = pMember;
                } else {
                    fs_free(pMember, NULL);
                }
            }
        }
        
        pCurrent = (pLineEnd < pBodyEnd && *pLineEnd == '\n') ? pLineEnd + 1 : pBodyEnd;
    }
    
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

    fs_file_writef(pFile, "\n  ],\n");
    
    /* Output enums */
    fs_file_writef(pFile, "  \"enums\": [\n");
    
    fsdoc_enum* pEnum;
    int isFirstEnum = 1;
    for (pEnum = pContext->pFirstEnum; pEnum != NULL; pEnum = pEnum->pNext) {
        if (!isFirstEnum) {
            fs_file_writef(pFile, ",\n");
        }
        isFirstEnum = 0;

        fs_file_writef(pFile, "    {\n");
        fs_file_writef(pFile, "      \"name\": ");
        fsdoc_write_json_string(pFile, pEnum->name);
        fs_file_writef(pFile, ",\n");
        fs_file_writef(pFile, "      \"description\": ");
        fsdoc_write_json_string(pFile, pEnum->pDescription ? pEnum->pDescription : "");
        fs_file_writef(pFile, ",\n");
        fs_file_writef(pFile, "      \"values\": [\n");
        
        fsdoc_enum_value* pValue;
        int isFirstValue = 1;
        for (pValue = pEnum->pFirstValue; pValue != NULL; pValue = pValue->pNext) {
            if (!isFirstValue) {
                fs_file_writef(pFile, ",\n");
            }
            isFirstValue = 0;
            
            fs_file_writef(pFile, "        {\n");
            fs_file_writef(pFile, "          \"name\": ");
            fsdoc_write_json_string(pFile, pValue->name);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"value\": ");
            fsdoc_write_json_string(pFile, pValue->value);
            fs_file_writef(pFile, ",\n");
            fs_file_writef(pFile, "          \"description\": ");
            fsdoc_write_json_string(pFile, pValue->pDescription ? pValue->pDescription : "");
            fs_file_writef(pFile, "\n");
            fs_file_writef(pFile, "        }");
        }
        
        fs_file_writef(pFile, "\n      ]\n");
        fs_file_writef(pFile, "    }");
    }
    
    fs_file_writef(pFile, "\n  ],\n");
    
    /* Output structs */
    fs_file_writef(pFile, "  \"structs\": [\n");
    
    fsdoc_struct* pStruct;
    int isFirstStruct = 1;
    for (pStruct = pContext->pFirstStruct; pStruct != NULL; pStruct = pStruct->pNext) {
        if (!isFirstStruct) {
            fs_file_writef(pFile, ",\n");
        }
        isFirstStruct = 0;

        fs_file_writef(pFile, "    {\n");
        fs_file_writef(pFile, "      \"name\": ");
        fsdoc_write_json_string(pFile, pStruct->name);
        fs_file_writef(pFile, ",\n");
        fs_file_writef(pFile, "      \"description\": ");
        fsdoc_write_json_string(pFile, pStruct->pDescription ? pStruct->pDescription : "");
        fs_file_writef(pFile, ",\n");
        fs_file_writef(pFile, "      \"isOpaque\": %s,\n", pStruct->isOpaque ? "true" : "false");
        fs_file_writef(pFile, "      \"members\": [\n");
        
        if (!pStruct->isOpaque) {
            fsdoc_struct_member* pMember;
            int isFirstMember = 1;
            for (pMember = pStruct->pFirstMember; pMember != NULL; pMember = pMember->pNext) {
                if (!isFirstMember) {
                    fs_file_writef(pFile, ",\n");
                }
                isFirstMember = 0;
                
                fs_file_writef(pFile, "        {\n");
                fs_file_writef(pFile, "          \"name\": ");
                fsdoc_write_json_string(pFile, pMember->name);
                fs_file_writef(pFile, ",\n");
                fs_file_writef(pFile, "          \"type\": ");
                fsdoc_write_json_string(pFile, pMember->type);
                fs_file_writef(pFile, ",\n");
                fs_file_writef(pFile, "          \"description\": ");
                fsdoc_write_json_string(pFile, pMember->pDescription ? pMember->pDescription : "");
                fs_file_writef(pFile, "\n");
                fs_file_writef(pFile, "        }");
            }
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
                
                /* Special case for variadic parameters */
                if (strcmp(pParam->type, "...") == 0) {
                    fs_file_writef(pFile, "**...**  \n");
                } else {
                    fs_file_writef(pFile, "**%s**  \n", pParam->name);
                }
                
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

    /* Output enums */
    if (pContext->pFirstEnum != NULL) {
        fsdoc_enum* pEnum;
        for (pEnum = pContext->pFirstEnum; pEnum != NULL; pEnum = pEnum->pNext) {
            fs_file_writef(pFile, "# enum %s\n\n", pEnum->name);
            
            /* Description */
            if (pEnum->pDescription != NULL && strlen(pEnum->pDescription) > 0) {
                fs_file_writef(pFile, "%s\n\n", pEnum->pDescription);
            }
            
            /* Values table */
            if (pEnum->pFirstValue != NULL) {
                /* First, check if any values have explicit assignments */
                int hasExplicitValues = 0;
                fsdoc_enum_value* pValue;
                for (pValue = pEnum->pFirstValue; pValue != NULL; pValue = pValue->pNext) {
                    if (strlen(pValue->value) > 0) {
                        hasExplicitValues = 1;
                        break;
                    }
                }
                
                if (hasExplicitValues) {
                    /* Two-column table: Name and Value */
                    fs_file_writef(pFile, "| Name | Value |\n");
                    fs_file_writef(pFile, "|------|-------|\n");
                    
                    for (pValue = pEnum->pFirstValue; pValue != NULL; pValue = pValue->pNext) {
                        fs_file_writef(pFile, "| `%s` | ", pValue->name);
                        if (strlen(pValue->value) > 0) {
                            fs_file_writef(pFile, "`%s`", pValue->value);
                        }
                        fs_file_writef(pFile, " |\n");
                    }
                } else {
                    /* Single-column table: Name only */
                    fs_file_writef(pFile, "| Name |\n");
                    fs_file_writef(pFile, "|------|\n");
                    
                    for (pValue = pEnum->pFirstValue; pValue != NULL; pValue = pValue->pNext) {
                        fs_file_writef(pFile, "| `%s` |\n", pValue->name);
                    }
                }
                
                fs_file_writef(pFile, "\n");
            }
            
            fs_file_writef(pFile, "---\n\n");
        }
    }

    /* Output structs */
    if (pContext->pFirstStruct != NULL) {
        fsdoc_struct* pStruct;
        for (pStruct = pContext->pFirstStruct; pStruct != NULL; pStruct = pStruct->pNext) {
            fs_file_writef(pFile, "# struct %s\n\n", pStruct->name);
            
            /* Description */
            if (pStruct->pDescription != NULL && strlen(pStruct->pDescription) > 0) {
                fs_file_writef(pFile, "%s\n\n", pStruct->pDescription);
            }
            
            if (pStruct->isOpaque) {
                fs_file_writef(pFile, "*Opaque.*\n\n");
            } else {
                /* Code block representation */
                if (pStruct->pFirstMember != NULL) {
                    /* First pass: find the maximum type length for alignment */
                    size_t maxTypeLen = 0;
                    fsdoc_struct_member* pMember;
                    for (pMember = pStruct->pFirstMember; pMember != NULL; pMember = pMember->pNext) {
                        /* For nested structs, calculate display length differently */
                        size_t typeLen;
                        if (strstr(pMember->type, "struct {") != NULL) {
                            /* For nested structs, use a fixed display length since they span multiple lines */
                            typeLen = strlen("struct { ... }");
                        } else {
                            typeLen = strlen(pMember->type);
                        }
                        if (typeLen > maxTypeLen) {
                            maxTypeLen = typeLen;
                        }
                    }
                    
                    /* Generate the struct code block */
                    fs_file_writef(pFile, "```c\n");
                    fs_file_writef(pFile, "struct %s\n", pStruct->name);
                    fs_file_writef(pFile, "{\n");
                    
                    for (pMember = pStruct->pFirstMember; pMember != NULL; pMember = pMember->pNext) {
                        /* Handle nested structs specially */
                        if (strstr(pMember->type, "struct {") != NULL) {
                            /* For nested structs, format them with opening brace on separate line */
                            /* Make a copy since strtok modifies the string */
                            char nestedTypeCopy[1024];
                            strcpy(nestedTypeCopy, pMember->type);
                            
                            char* line = strtok(nestedTypeCopy, "\n");
                            int isFirstLine = 1;
                            
                            while (line != NULL) {
                                if (isFirstLine) {
                                    /* First line: "struct {" - split into "struct" and "{" */
                                    fs_file_writef(pFile, "    struct\n");
                                    fs_file_writef(pFile, "    {\n");
                                    isFirstLine = 0;
                                } else if (strstr(line, "}") != NULL) {
                                    /* Last line: "    }" - add member name with just one space */
                                    fs_file_writef(pFile, "    } %s;\n", pMember->name);
                                } else {
                                    /* Middle lines: member definitions - reduce indentation from 8 to 4 spaces */
                                    /* Remove the extra 4 spaces of indentation */
                                    char* trimmedLine = line;
                                    if (strncmp(line, "        ", 8) == 0) {
                                        trimmedLine = line + 4; /* Remove 4 extra spaces, keep 4 */
                                    }
                                    fs_file_writef(pFile, "    %s\n", trimmedLine);
                                }
                                line = strtok(NULL, "\n");
                            }
                        } else {
                            /* Regular member */
                            /* Calculate padding needed for alignment */
                            size_t typeLen = strlen(pMember->type);
                            size_t padding = maxTypeLen - typeLen;
                            
                            fs_file_writef(pFile, "    %s", pMember->type);
                            
                            /* Add padding spaces */
                            for (size_t i = 0; i < padding; i++) {
                                fs_file_writef(pFile, " ");
                            }
                            
                            fs_file_writef(pFile, " %s;\n", pMember->name);
                        }
                    }
                    
                    fs_file_writef(pFile, "};\n");
                    fs_file_writef(pFile, "```\n\n");
                }
            }
            
            fs_file_writef(pFile, "---\n\n");
        }
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
                        int in_code_block = 0;
                        int paragraph_break_pending = 0;
                        int just_closed_code_block = 0;
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
                                    /* Empty line indicates paragraph break */
                                    paragraph_break_pending = 1;
                                    pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                                    continue;
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
                                
                                /* Check if this line looks like code (indented or starts with fs_) */
                                int is_code_line = 0;
                                if (line[0] == ' ' && line[1] == ' ' && line[2] == ' ' && line[3] == ' ') {
                                    is_code_line = 1;
                                } else if (strstr(line, "fs_") == line) {
                                    is_code_line = 1;
                                }
                                
                                /* Handle code blocks */
                                if (is_code_line && !in_code_block) {
                                    if (!first_desc_line) {
                                        if (paragraph_break_pending) {
                                            strcpy(pWrite, "<br><br>");
                                            pWrite += 8;
                                            paragraph_break_pending = 0;
                                        } else {
                                            strcpy(pWrite, " ");
                                            pWrite++;
                                        }
                                    }
                                    strcpy(pWrite, "<br>    ");
                                    pWrite += 8;
                                    in_code_block = 1;
                                } else if (!is_code_line && in_code_block) {
                                    strcpy(pWrite, "<br>");
                                    pWrite += 4;
                                    in_code_block = 0;
                                    just_closed_code_block = 1;
                                }
                                
                                /* This is part of the description */
                                if (!first_desc_line && !is_code_line && !in_code_block) {
                                    if (just_closed_code_block) {
                                        /* Just add a single line break after code block */
                                        strcpy(pWrite, "<br>");
                                        pWrite += 4;
                                        just_closed_code_block = 0;
                                    } else if (paragraph_break_pending) {
                                        strcpy(pWrite, "<br><br>");
                                        pWrite += 8;
                                        paragraph_break_pending = 0;
                                    } else {
                                        /* Check if this line starts a new logical paragraph */
                                        if (strstr(line, "Transparent mode is") == line ||
                                            strstr(line, "Furthermore,") == line ||
                                            strstr(line, "Here the archive") == line) {
                                            strcpy(pWrite, "<br><br>");
                                            pWrite += 8;
                                        } else {
                                            strcpy(pWrite, " ");
                                            pWrite++;
                                        }
                                    }
                                } else if (is_code_line && in_code_block && !first_desc_line) {
                                    strcpy(pWrite, "<br>    ");
                                    pWrite += 8;
                                }
                                
                                /* Trim leading spaces from code lines */
                                const char* line_content = line;
                                if (is_code_line && line[0] == ' ') {
                                    while (*line_content == ' ') line_content++;
                                }
                                
                                /* Copy line content while escaping pipe symbols for table compatibility */
                                const char* pSrc = line_content;
                                while (*pSrc != '\0') {
                                    if (*pSrc == '|') {
                                        strcpy(pWrite, "\\|");
                                        pWrite += 2;
                                    } else {
                                        *pWrite = *pSrc;
                                        pWrite++;
                                    }
                                    pSrc++;
                                }
                                first_desc_line = 0;
                                paragraph_break_pending = 0;
                            }
                            
                            pCurrent = (*pDescEnd == '\n') ? pDescEnd + 1 : pDescEnd;
                        }
                        
                        /* Close any open code block */
                        if (in_code_block) {
                            strcpy(pWrite, "<br>");
                            pWrite += 4;
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

static void fsdoc_free_enum_values(fsdoc_enum_value* pValue)
{
    fsdoc_enum_value* pCurrent;
    fsdoc_enum_value* pNext;

    pCurrent = pValue;
    while (pCurrent != NULL) {
        pNext = pCurrent->pNext;
        
        if (pCurrent->pDescription != NULL) {
            fs_free(pCurrent->pDescription, NULL);
        }
        
        fs_free(pCurrent, NULL);
        pCurrent = pNext;
    }
}

static void fsdoc_free_enum(fsdoc_enum* pEnum)
{
    if (pEnum == NULL) {
        return;
    }

    if (pEnum->pDescription != NULL) {
        fs_free(pEnum->pDescription, NULL);
    }

    fsdoc_free_enum_values(pEnum->pFirstValue);
    fs_free(pEnum, NULL);
}

static void fsdoc_free_struct_members(fsdoc_struct_member* pMember)
{
    fsdoc_struct_member* pCurrent;
    fsdoc_struct_member* pNext;

    pCurrent = pMember;
    while (pCurrent != NULL) {
        pNext = pCurrent->pNext;
        
        if (pCurrent->pDescription != NULL) {
            fs_free(pCurrent->pDescription, NULL);
        }
        
        fs_free(pCurrent, NULL);
        pCurrent = pNext;
    }
}

static void fsdoc_free_struct(fsdoc_struct* pStruct)
{
    if (pStruct == NULL) {
        return;
    }

    if (pStruct->pDescription != NULL) {
        fs_free(pStruct->pDescription, NULL);
    }

    fsdoc_free_struct_members(pStruct->pFirstMember);
    fs_free(pStruct, NULL);
}

static void fsdoc_free_full_struct_names(fsdoc_full_struct_name* pName)
{
    while (pName != NULL) {
        fsdoc_full_struct_name* pNext = pName->pNext;
        fs_free(pName, NULL);
        pName = pNext;
    }
}

static int fsdoc_scan_for_full_struct_definitions(fsdoc_context* pContext)
{
    char* pCurrent;
    
    if (pContext == NULL || pContext->pFileContent == NULL) {
        return 1;
    }
    
    pCurrent = pContext->pFileContent;
    
    while (*pCurrent != '\0') {
        char* pStructLine = strstr(pCurrent, "typedef struct");
        char* pPlainStructLine = strstr(pCurrent, "struct ");
        
        /* Find the earliest one */
        char* pEarliest = NULL;
        int isTypedef = 0;
        
        if (pStructLine != NULL && pPlainStructLine != NULL) {
            if (pStructLine < pPlainStructLine) {
                pEarliest = pStructLine;
                isTypedef = 1;
            } else {
                pEarliest = pPlainStructLine;
                isTypedef = 0;
            }
        } else if (pStructLine != NULL) {
            pEarliest = pStructLine;
            isTypedef = 1;
        } else if (pPlainStructLine != NULL) {
            pEarliest = pPlainStructLine;
            isTypedef = 0;
        } else {
            break;
        }
        
        /* Check if this has a full definition (contains braces) */
        char* pSemicolon = strchr(pEarliest, ';');
        char* pOpenBrace = strchr(pEarliest, '{');
        
        if (pOpenBrace != NULL && (pSemicolon == NULL || pOpenBrace < pSemicolon)) {
            /* This is a full definition - extract the struct name */
            char* pNameStart;
            char* pNameEnd;
            
            if (isTypedef) {
                /* For typedef struct { ... } name; find name after closing brace */
                char* pCloseBrace = pOpenBrace;
                int braceCount = 0;
                
                while (*pCloseBrace != '\0') {
                    if (*pCloseBrace == '{') {
                        braceCount++;
                    } else if (*pCloseBrace == '}') {
                        braceCount--;
                        if (braceCount == 0) {
                            break;
                        }
                    }
                    pCloseBrace++;
                }
                
                if (*pCloseBrace == '}') {
                    /* Find the struct name after the closing brace */
                    pNameStart = pCloseBrace + 1;
                    while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
                        pNameStart++;
                    }
                    
                    pNameEnd = pNameStart;
                    while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != ';') {
                        pNameEnd++;
                    }
                }
            } else {
                /* For struct name { ... }; find name before opening brace */
                pNameStart = pEarliest + 7; /* strlen("struct ") */
                while (*pNameStart != '\0' && FSDOC_IS_SPACE(*pNameStart)) {
                    pNameStart++;
                }
                
                pNameEnd = pNameStart;
                while (*pNameEnd != '\0' && !FSDOC_IS_SPACE(*pNameEnd) && *pNameEnd != '{') {
                    pNameEnd++;
                }
            }
            
            /* Add this struct name to our list */
            if (pNameEnd > pNameStart) {
                size_t nameLen = pNameEnd - pNameStart;
                if (nameLen < 256) {
                    fsdoc_full_struct_name* pNewName = fs_malloc(sizeof(fsdoc_full_struct_name), NULL);
                    if (pNewName != NULL) {
                        strncpy(pNewName->name, pNameStart, nameLen);
                        pNewName->name[nameLen] = '\0';
                        pNewName->pNext = pContext->pFirstFullStructName;
                        pContext->pFirstFullStructName = pNewName;
                    }
                }
            }
        }
        
        pCurrent = pEarliest + (isTypedef ? 14 : 7); /* Move past the keyword */
    }
    
    return 0;
}

static int fsdoc_is_struct_fully_defined(fsdoc_context* pContext, const char* pStructName)
{
    fsdoc_full_struct_name* pCurrent;
    
    if (pContext == NULL || pStructName == NULL) {
        return 0;
    }
    
    for (pCurrent = pContext->pFirstFullStructName; pCurrent != NULL; pCurrent = pCurrent->pNext) {
        if (strcmp(pCurrent->name, pStructName) == 0) {
            return 1;
        }
    }
    
    return 0;
}
