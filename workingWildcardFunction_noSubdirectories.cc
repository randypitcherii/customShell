void expandWildcardsIfNecessary(char * arg) {
	//Return if arg does not contain '*' or '?'
	if (strchr(arg, '*') == NULL && strchr(arg, '?') == NULL) {
		Command::_currentSimpleCommand->insertArgument(strdup(arg));
		return;
	}
	
	// 1. Convert wildcard to regex
	char * reg = (char *) malloc(2*strlen(arg) + 10);
	char * a = arg;
	char * r = reg;
	
	//match beginning of line
	*r = '^';
	r++;
	
	//match content of a
	while (*a) {
		if (*a == '*') { *r='.'; r++; *r='*'; r++; }
		else if (*a == '?') { *r='.'; r++;}
		else if (*a == '.') { *r='\\'; r++; *r='.'; r++;}
		else { *r=*a; r++;}
		a++;
	}
	
	//match end of line
	*r = '$';
	r++;
	*r = '\0';
	
	// 2. compile regex
	char regExpComplete[1024];
	sprintf(regExpComplete, "%s", reg);
	regex_t regex;
	int result = regcomp( &regex, regExpComplete,  REG_EXTENDED|REG_NOSUB);
	
	if (result != 0) {
		perror("Wildcard regex compilation error");
		return;
	}
	
	// 3. List directory and add as arguments the entries that match the regex
	DIR * dir = opendir(".");
	if (dir == NULL) {
		perror("Wildcard open directory error.");
		return;
	}
	
	struct dirent* ent;
	int maxEntries = 20;
	int nEntries = 0;
	char ** array = (char **) malloc(maxEntries * sizeof(char*));
	
	while ((ent = readdir(dir)) != NULL) {
		if (regexec(&regex, ent->d_name, 0,NULL, 0) == 0) {
			if (ent->d_name[0] == '.' && arg[0] != '.') {
				continue;//don't add dot files unless arg starts with a '.'
			}
			
			if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, ".")) {
				//exclude the '.' and '..' arguments from wildcarding
				continue;
			}
			if (nEntries == maxEntries) {
				maxEntries *= 2;
				array = (char **) realloc(array, maxEntries*sizeof(char*));
			}
			array[nEntries] = strdup(ent->d_name);
			nEntries++;
		}
	}
	closedir(dir);
	
	//sort the array
	qsort((void *) array, nEntries, sizeof(char*), compareFunction);
	
	for (int i = 0; i < nEntries; i++) {
		Command::_currentSimpleCommand->insertArgument(strdup(array[i]));
	}
	
	free(array);
}
