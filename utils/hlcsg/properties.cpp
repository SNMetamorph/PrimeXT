//KGP -- added in for use with HLCSG_NULLIFY_INVISIBLE

#include "csg.h"

#ifdef HLCSG_NULLIFY_INVISIBLE
#include <fstream>
#include <istream>
using namespace std;

set< string > g_invisible_items;

void properties_initialize(const char* filename)
{
    if (filename == NULL)
    { return; }

    if (q_exists(filename))
    { Log("Loading null entity list from '%s'\n", filename); }
    else
    {
		Error("Could not find null entity list file '%s'\n", filename);
        return;
    }

	ifstream file(filename,ios::in);
	if(!file)
	{ 
		file.close();
		return; 
	}


	//begin reading list of items
	char line[MAX_VAL]; //MAX_VALUE //vluzacn
	memset(line,0,sizeof(char)*4096);
	while(!file.eof())
	{
		string str;
		getline(file,str);
		{ //--vluzacn
			char *s = strdup (str.c_str ());
			int i;
			for (i = 0; s[i] != '\0'; i++)
			{
				if (s[i] == '\n' || s[i] == '\r')
				{
					s[i] = '\0';
				}
			}
			str.assign (s);
			free (s);
		}
		if(str.size() < 1)
		{ continue; }
		g_invisible_items.insert(str);
	}
	file.close();
}

#endif
