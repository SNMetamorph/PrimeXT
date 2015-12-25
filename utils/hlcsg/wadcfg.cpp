// AJM: ADDED THIS ENTIRE FILE IN

#include "csg.h"
#ifdef HLCSG_WADCFG_NEW
void LoadWadconfig (const char *filename, const char *configname)
{
	Log ("Loading wad configuration '%s' from '%s' :\n", configname, filename);
	int found = 0;
	int count = 0;
	int size;
	char *buffer;
	size = LoadFile (filename, &buffer);
	ParseFromMemory (buffer, size);
	while (GetToken (true))
	{
		bool skip = true;
		if (!strcasecmp (g_token, configname))
		{
			skip = false;
			found++;
		}
		if (!GetToken (true) || strcasecmp (g_token, "{"))
		{
			Error ("parsing '%s': missing '{'.", filename);
		}
		while (1)
		{
			if (!GetToken (true))
			{
				Error ("parsing '%s': unexpected end of file.", filename);
			}
			if (!strcasecmp (g_token, "}"))
			{
				break;
			}
			if (skip)
			{
				continue;
			}
			Log (" ");
			bool include = false;
			if (!strcasecmp (g_token, "include"))
			{
				Log ("include ");
				include = true;
				if (!GetToken (true))
				{
					Error ("parsing '%s': unexpected end of file.", filename);
				}
			}
			Log ("\"%s\"\n", g_token);
			if (g_iNumWadPaths >= MAX_WADPATHS)
			{
				Error ("parsing '%s': too many wad files.", filename);
			}
			count++;
#ifdef HLCSG_AUTOWAD_NEW
			PushWadPath (g_token, !include);
#else
			wadpath_t *current = (wadpath_t *)malloc (sizeof (wadpath_t));
			hlassume (current != NULL, assume_NoMemory);
			g_pWadPaths[g_iNumWadPaths] = current;
			g_iNumWadPaths++;
			safe_strncpy (current->path, g_token, _MAX_PATH);
			current->usedbymap = true; // what's this?
			current->usedtextures = 0;
			if (include)
			{
				Developer (DEVELOPER_LEVEL_MESSAGE, "LoadWadcfgfile: including '%s'.\n", current->path);
				g_WadInclude.push_back(current->path);
			}
#endif
		}
	}
	if (found == 0)
	{
		Error ("Couldn't find wad configuration '%s' in file '%s'.\n", configname, filename);
	}
	if (found >= 2)
	{
		Error ("Found more than one wad configuration for '%s' in file '%s'.\n", configname, filename);
	}
	free (buffer); // should not be freed because it is still being used as script buffer
	//Log ("Using custom wadfile configuration: '%s' (with %i wad%s)\n", configname, count, count > 1 ? "s" : "");
}
void LoadWadcfgfile (const char *filename)
{
	Log ("Loading wad configuration file '%s' :\n", filename);
	int count = 0;
	int size;
	char *buffer;
	size = LoadFile (filename, &buffer);
	ParseFromMemory (buffer, size);
	while (GetToken (true))
	{
		Log (" ");
		bool include = false;
		if (!strcasecmp (g_token, "include"))
		{
			Log ("include ");
			include = true;
			if (!GetToken (true))
			{
				Error ("parsing '%s': unexpected end of file.", filename);
			}
		}
		Log ("\"%s\"\n", g_token);
		if (g_iNumWadPaths >= MAX_WADPATHS)
		{
			Error ("parsing '%s': too many wad files.", filename);
		}
		count++;
#ifdef HLCSG_AUTOWAD_NEW
		PushWadPath (g_token, !include);
#else
		wadpath_t *current = (wadpath_t *)malloc (sizeof (wadpath_t));
		hlassume (current != NULL, assume_NoMemory);
		g_pWadPaths[g_iNumWadPaths] = current;
		g_iNumWadPaths++;
		safe_strncpy (current->path, g_token, _MAX_PATH);
		current->usedbymap = true; // what's this?
		current->usedtextures = 0;
		if (include)
		{
			Developer (DEVELOPER_LEVEL_MESSAGE, "LoadWadcfgfile: including '%s'.\n", current->path);
			g_WadInclude.push_back(current->path);
		}
#endif
	}
	free (buffer); // should not be freed because it is still being used as script buffer
	//Log ("Using custom wadfile configuration: '%s' (with %i wad%s)\n", filename, count, count > 1 ? "s" : "");
}
#else
#ifdef HLCSG_WADCFG

//#ifdef SYSTEM_WIN32
#if defined(SYSTEM_WIN32) && !defined( __GNUC__ )
#   include "atlbase.h" // win32 registry API
#else
char *g_apppath = NULL; //JK: Stores application path
#endif

#define WAD_CONFIG_FILE "wad.cfg"

typedef struct wadname_s 
{
    char                    wadname[_MAX_PATH];
    bool                    wadinclude;
    struct wadname_s*       next;
} wadname_t; 

typedef struct wadconfig_s
{
    char                    name[_MAX_PATH];
    int                     entries;
    wadname_t*              firstentry;
    wadconfig_s*            next;
} wadconfig_t;

wadconfig_t* g_WadCfg;  // anchor for the wadconfigurations linked list

bool        g_bWadConfigsLoaded = false;
int         g_wadcfglinecount = 1;

//JK: added in
char *g_wadcfgfile = NULL;

// little helper function
void stripwadpath (char *str)
{
    char *p = str + strlen (str) - 1;
    while ((*p == ' ' || *p == '\n' || *p == '\t') && p >= str) *p-- = '\0';
}

// =====================================================================================
//  WadCfgParseError
// =====================================================================================
void        WadCfgParseError(const char* message, int linenum, char* got)
{
    stripwadpath(got); // strip newlines

    Log("Error parsing " WAD_CONFIG_FILE ", line %i:\n"
        "%s, got '%s'\n", linenum, message, got);

    Log("If you need help on usage of the wad.cfg file, be sure to check http://www.zhlt.info/using-wad.cfg.html that came"
        " in the zip file with these tools.\n"); 

    g_bWadConfigsLoaded = false;
}

// ============================================================================
//  IsWhitespace
// ============================================================================
bool        IsWhitespace(const char ThinkChar)
{
    if ((ThinkChar == ' ') || (ThinkChar == '\t') || (ThinkChar == '\n') || (ThinkChar == 13) /* strange whitespace char */)
        return true;

    return false;
}

// ============================================================================
//  Safe_GetToken
// ============================================================================
void        Safe_GetToken(FILE* source, char* TokenBuffer, const unsigned int MaxBufferLength)
{
    char    ThinkChar[1];       // 2 char array = thinkchar and null terminator
    bool    InToken = false;    // are we getting a token?
    bool    InQuotes = false;   // are we in quotes?
    bool    InComment = false;  // are we in a comment (//)?
    //fpos_t* sourcepos;
    long    sourcepos;

    TokenBuffer[0] = '\0';
    ThinkChar[1] = '\0';
    
    while(!feof(source))
    {
        if (strlen(TokenBuffer) >= MaxBufferLength)
            return;     // we cant add any more chars onto the buffer, its maxed out

        ThinkChar[0] = fgetc(source);   
        
        if (ThinkChar[0] == '\n')              // newline
        {
            g_wadcfglinecount++;
            InComment = false;
        }

        if (ThinkChar[0] == 'я')
            return;  
                
        if (IsWhitespace(ThinkChar[0]) && !InToken)
            continue;   // whitespace before token, ignore

        if (ThinkChar[0] == '"')               // quotes
        {
            if(InQuotes)
                InQuotes = false;
            else
                InQuotes = true;
            continue;   // dont include quotes
        }

        if (ThinkChar[0] == '/')             
        {   
            sourcepos = ftell(source);
            // might be a comment, see if the next char is a forward slash
            if (fgetc(source) == '/')    // if it is, were in a comment
            {
                InComment = true;
                continue;
            }
            else // if not, rewind pretend nothing happened...
            {
                fseek(source, sourcepos, 0);
            }
        }

        if (
             (IsWhitespace(ThinkChar[0]) && InToken && !InQuotes) // whitespace AFTER token and not in quotes
          || (InToken && InComment) // we hit a comment, and we have our token
           )
        {
            //printf("[gt: %s]\n", TokenBuffer);
            return;   
        }

        if (!InComment)
        {
            strcat(TokenBuffer, ThinkChar);
            InToken = true;
        }
    }
}

// =====================================================================================
//  GetWadConfig
//      return true if we didnt encounter any fatal errors
#define MAX_TOKENBUFFER _MAX_PATH
// =====================================================================================
bool        GetWadConfig(FILE* wadcfg, wadconfig_t* wadconfig)
{
    char            TokenBuffer[MAX_TOKENBUFFER];
    wadname_t*      current;
    wadname_t*      previous;

    while (!feof(wadcfg))
    {
        Safe_GetToken(wadcfg, TokenBuffer, MAX_TOKENBUFFER);

        if (!strcmp(TokenBuffer, "}"))
            return true; // no more work to do
        
        if (!strcmp(TokenBuffer, ";"))
            continue; // old seperator, no longer used but here for backwards compadibility

        if (!strcmp(TokenBuffer, "{"))  // wtf
        {
            WadCfgParseError("Expected wadpath (Nested blocks illegal)", g_wadcfglinecount, TokenBuffer);
            return false;
        }

        // otherwise assume its a wadpath, make an entry in this configuration
        current = (wadname_t*)malloc(sizeof(wadname_t));
        wadconfig->entries++;
        current->next = NULL;
        current->wadinclude = false;

        if (!strcmp(TokenBuffer, "include"))
        {
            current->wadinclude = true;
            Safe_GetToken(wadcfg, TokenBuffer, MAX_TOKENBUFFER);
        }
        
        strcpy_s(current->wadname, TokenBuffer);
        
        if (!wadconfig->firstentry)
        {
            wadconfig->firstentry = current;
        }
        else
        {
            previous->next = current;
        }

        previous = current;
        previous->next = NULL;
    }

    safe_snprintf(TokenBuffer, MAX_TOKENBUFFER, "Unexptected end of file encountered while parsing configuration '%s'", wadconfig->name);
    WadCfgParseError(TokenBuffer, g_wadcfglinecount, "(eof)");
    return false; 
}
#undef MAX_TOKENBUFFER

// =====================================================================================
//  LoadWadConfigFile
// =====================================================================================
void        LoadWadConfigFile()
{
    FILE*           wadcfg;
    wadconfig_t*    current;
    wadconfig_t*    previous;

    char            temp[_MAX_PATH];

    //JK: If exists lets use user-defined file
    if (g_wadcfgfile && q_exists(g_wadcfgfile))
    {
    	wadcfg = fopen(g_wadcfgfile, "r");
    }
    else // find the wad.cfg file
    {
        char    appdir[_MAX_PATH];
        char    tmp[_MAX_PATH];
                
        memset(tmp, 0, sizeof(tmp));
        memset(appdir, 0, sizeof(appdir));

        // Get application directory (only an approximation on posix systems)
        // try looking in the directory we were run from
        #ifdef SYSTEM_WIN32
        GetModuleFileName(NULL, tmp, _MAX_PATH);
        #else
        safe_strncpy(tmp, g_apppath, _MAX_PATH);
        #endif
    
        ExtractFilePath(tmp, appdir);
        safe_snprintf(tmp, _MAX_PATH, "%s%s", appdir, WAD_CONFIG_FILE);

        #ifdef _DEBUG
        Log("[dbg] Trying '%s'\n", tmp);
        #endif

        if (!q_exists(tmp))
        {
            // try the Half-Life directory
            /*#ifdef SYSTEM_WIN32
            {
                HKEY        HLMachineKey;
                DWORD       disposition;
                DWORD       dwType, dwSize;

                // REG: create machinekey
                RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Valve\\Half-Life"), 0, NULL,  
                                                        0, 0, NULL, &HLMachineKey, &disposition);
                // REG: get hl dir 
                dwType = REG_SZ;
                dwSize = _MAX_PATH;
                RegQueryValueEx(HLMachineKey, TEXT("InstallPath"), NULL, &dwType, (PBYTE)&appdir, &dwSize);
            }
            
            safe_strncpy(tmp, appdir, _MAX_PATH);
            safe_strncat(tmp, SYSTEM_SLASH_STR, _MAX_PATH); // system slash pointless as this will only work on win32, but anyway...
            safe_strncat(tmp, WAD_CONFIG_FILE, _MAX_PATH);

            #ifdef _DEBUG
            Log("[dbg] Trying '%s'\n", tmp);
           #endif

            if (!q_exists(tmp))
            #endif  // SYSTEM_WIN32*/ // not a good idea. --vluzacn
            {
                // still cant find it, error out
                Log("Warning: could not find wad configurations file\n"
                    /*"Make sure that wad.cfg is in the Half-Life directory or the current working directory\n"*/
                   );
                return;
            }
        }

        wadcfg = fopen(tmp, "r");
    }

    if (!wadcfg)
    {
        // cant open it, die
        Log("Warning: could not open the wad configurations file\n"
            "Make sure that wad.cfg is in the Half-Life directory or the current working directory\n"
            );
        return;
    }

    while(!feof(wadcfg))
    {
        Safe_GetToken(wadcfg, temp, MAX_WAD_CFG_NAME);

        if (!strcmp(temp, "HalfLifePath="))  // backwards compadibitly
        {
            Safe_GetToken(wadcfg, temp, MAX_WAD_CFG_NAME); 
            Warning("Redundant line in " WAD_CONFIG_FILE ": \"HalfLifePath= %s\" - Ignoring...\n",
                temp);
            continue;
        }

        if (feof(wadcfg))
            break;

        // assume its the name of a configuration
        current = (wadconfig_t*)malloc(sizeof(wadconfig_t));
        safe_strncpy(current->name, temp, _MAX_PATH);
        current->entries = 0;
        current->next = NULL;
        current->firstentry = NULL;

        // make sure the next char starts a wad configuration
        Safe_GetToken(wadcfg, temp, _MAX_PATH);
        if (strcmp(temp, "{"))
        {
            WadCfgParseError("Expected start of wadfile configuration, '{'", g_wadcfglinecount, temp);
            //Log("[dbg] temp[0] is %i\n", temp[0]);
            fclose(wadcfg);
            return;
        }

        // otherwise were ok, get the definition
        if (!GetWadConfig(wadcfg, current))
        {
            fclose(wadcfg);
            return;
        }

        // add this config to the list
        if (!g_WadCfg)
        {
            g_WadCfg = current;
        }
        else
        {
            previous->next = current;
        }

        previous = current;
        previous->next = NULL;
    }

    g_bWadConfigsLoaded = true;
}

// =====================================================================================
//  ProcessWadConfiguration
// =====================================================================================
void        ProcessWadConfiguration()
{
    int             usedwads = 0;
    char            szTmp[1024]; // arbitrary, but needs to be large. probably bigger than this.
    wadconfig_t*    config;
    wadname_t*      path;

    if(!g_bWadConfigsLoaded) // we did a graceful exit due to some warning/error, so dont complain about it
    {
        Log("Using mapfile wad configuration\n");
        return;
    }

    szTmp[0] = 0;
    config = g_WadCfg;

    if (!wadconfigname)
        return; // this should never happen

    if (!config)
    {
        Warning("No configurations detected in wad.cfg\n"
                "using map wad configuration");
        return;
    }

    while (1)
    {
        if (!strcmp(config->name, wadconfigname))
        {
            path = config->firstentry;
            while (1)
            {
                if (!path)
                    break;

                Verbose("Wadpath from wad.cfg: '%s'\n", path->wadname);
                PushWadPath(path->wadname, true);
                safe_snprintf(szTmp, 1024, "%s%s;", szTmp, path->wadname);
                usedwads++;

                if (path->wadinclude)
                    g_WadInclude.push_back(path->wadname);

                // next path
                path = path->next;
            }
            break;  // the user can only specify one wad configuration, were done here
        }

        // next config
        config = config->next;
        if (!config)
            break;
    }
    
    if (usedwads)
    {
        Log("Using custom wadfile configuration: '%s' (with %i wad%s)\n", wadconfigname, usedwads, usedwads > 1 ? "s" : "");
        SetKeyValue(&g_entities[0], "wad", szTmp);
        SetKeyValue(&g_entities[0], "_wad", szTmp);    
    }
    else
    {
        Warning("no wadfiles are specified in configuration '%s' --\n"
                "Using map wadfile configuration", wadconfigname);
        g_bWadConfigsLoaded = false;
    }

    return;
}

// =====================================================================================
//  WadCfg_cleanup
// =====================================================================================
void        WadCfg_cleanup()
{
    wadconfig_t*    config;
    wadconfig_t*    nextconfig;
    wadname_t*      path;
    wadname_t*      nextpath;

    config = g_WadCfg;
    while (config)
    {
        path = config->firstentry;
        while (path)
        {
            nextpath = path->next;
            free(path);
            path = nextpath;
        }

        nextconfig = config->next;
        free(config);
        config = nextconfig;
    }

	g_WadCfg = NULL;
	g_bWadConfigsLoaded = false;
    return;
}



#endif // HLCSG_WADCFG
#endif

