#pragma once
#define MACRO_TO_STRING2(s)     #s
#define MACRO_TO_STRING(s)      MACRO_TO_STRING2(s)

#define APP_ABBREVIATION        "pxrad"
#define APP_TITLE_STR           "PrimeXT Tools [" APP_ABBREVIATION  "]"
#define APP_GITHUB_LINK         "https://github.com/SNMetamorph/PrimeXT"
#define APP_BUILD_DATE          __DATE__ " " __TIME__
#define APP_VERSION_MAJOR       1
#define APP_VERSION_MINOR       0
#define APP_VERSION_STRING      MACRO_TO_STRING(APP_VERSION_MAJOR)      \
                                "." MACRO_TO_STRING(APP_VERSION_MINOR)  \
                                "\0"
#define APP_VERSION_STRING2     MACRO_TO_STRING(APP_VERSION_MAJOR)      \
                                "." MACRO_TO_STRING(APP_VERSION_MINOR)
