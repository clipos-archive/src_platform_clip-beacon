// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright © 2015-2018 ANSSI. All Rights Reserved.
/*
 * Copyright 2015 ANSSI
 * Author: Hugo Chargois <clipos@ssi.gouv.fr>
 * All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define LOG(fmt, args...) \
	fprintf(stderr, fmt "\n", ##args)

#define DIE(fmt, args...) \
	do { \
		LOG(fmt, ##args); \
		exit(EXIT_FAILURE); \
	} while (0)

char *get_first_line_of_file(char *filename)
{
	FILE *f;
	char *buf = NULL;
	size_t buf_len = 0;
	size_t str_len;

	if (!(f = fopen(filename, "r")))
		DIE("Unable to open %s (%s)", filename, strerror(errno));
	if (getline(&buf, &buf_len, f) < 0)
		DIE("Unable to read %s (%s)", filename, strerror(errno));
	fclose(f);
	str_len = strnlen(buf, buf_len);
	if (buf[str_len-1] == '\n')
		buf[str_len-1] = '\0';
	return buf;
}

char *get_url()
{
	return get_first_line_of_file(BEACON_URL_FILE);
}

char *path_concat(char *path1, char* path2)
{
	size_t concat_len;
	char *concat;

	concat_len = snprintf(0, 0, "%s/%s", path1, path2);
	concat = malloc(concat_len + 1);
	if (!concat)
		DIE("Oops, malloc failed. Bye bye");
	snprintf(concat, concat_len + 1, "%s/%s", path1, path2);

	return concat;
}

int create_form(struct curl_httppost **form, struct curl_httppost **form_end)
{
	DIR *dp;
	struct dirent *ep;
	int nfiles = 0;

	if ((dp = opendir(BEACON_REPORT_DIR)) == NULL)
		DIE("Unable to open directory %s (%s)",
				BEACON_REPORT_DIR, strerror(errno));

	while ((ep = readdir(dp))) {
		char *full_name;
		struct stat statbuf;

		full_name = path_concat(BEACON_REPORT_DIR, ep->d_name);

		if (stat(full_name, &statbuf) < 0)
			DIE("Cannot stat %s (%s)", full_name, strerror(errno));
		if (S_ISDIR(statbuf.st_mode))
			continue;

		nfiles++;
		curl_formadd(
				form,
				form_end,
				CURLFORM_COPYNAME, ep->d_name,
				CURLFORM_FILE, full_name,
				CURLFORM_CONTENTTYPE, "application/octet-stream",
				CURLFORM_END);
	}
	return nfiles;
}

void test_file_accessible(char *fn, int dir)
{
	// for a simple file, we only check read status, for a dir, we
	// also check that it is executable
	int mode = dir ? R_OK|X_OK : R_OK;
	if (access(fn, mode))
		LOG("Error accessing %s %s (%s)",
				dir ? "directory" : "file",
				fn,
				strerror(errno)
		   );
}

size_t blackhole_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

int main(void)
{
	CURL *curl;
	CURLcode res;
	char errormsg[CURL_ERROR_SIZE];
	char *url;
	struct curl_httppost *form = NULL;
	struct curl_httppost *form_end = NULL;
	struct curl_slist *headers = NULL;
	int nfiles;
	long http_status = 0;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if (!curl)
		DIE("Failed to initialize libcurl");

	url = get_url();
	nfiles = create_form(&form, &form_end);

	// Provide useful error messages that explain why curl will
	// (probably ?) fail...
	test_file_accessible(BEACON_CERT_FILE, 0);
	test_file_accessible(BEACON_KEY_FILE, 0);
	test_file_accessible(BEACON_CA_PATH, 1);

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errormsg);
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(curl, CURLOPT_SSLCERT, BEACON_CERT_FILE);
	curl_easy_setopt(curl, CURLOPT_SSLKEY, BEACON_KEY_FILE);
	curl_easy_setopt(curl, CURLOPT_CAPATH, BEACON_CA_PATH);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, blackhole_write_callback);
	// Squid doesn't support "Expect: 100-continue" behavior, we remove it
	headers = curl_slist_append(headers, "Expect:");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// bad but required because we use stunnel as the TLS endpoint,
	// which doesn't have the right certificate...
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	LOG("Sending %d file(s) to %s", nfiles, url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		DIE("HTTP request failed : %s (%s)", curl_easy_strerror(res), errormsg);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
	if (http_status < 200 || http_status >= 300)
		DIE("HTTP request got a %ld response", http_status);

	curl_easy_cleanup(curl);
	LOG("Done, exiting");
	return EXIT_SUCCESS;
}
