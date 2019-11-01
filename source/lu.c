/*
 * Copyright (c) 2019, AT&T Intellectual Property.  All rights reserved.
 * Copyright (c) 2016 by Brocade Communications Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#define	_GNU_SOURCE
#define	LOGINUID_PATH	"/proc/self/loginuid"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(void)
{
	printf("\nUsage: ");
	printf("/opt/vyatta/sbin/lu --user <username> [--setprivs] command\n");
	printf("  Options:\n");
	printf("    --setprivs: Set privileges\n");
	printf("    --user: Username to execute as\n");
}

static int update_loginuid(uid_t uid)
{
	char *str_uid = NULL;
	int fd = -1, rc = -1;

	fd = open(LOGINUID_PATH, O_NOFOLLOW | O_RDWR | O_TRUNC);
	if (fd < 0) {
		fprintf(stderr,
			"lu: can't open file /proc/self/loginuid: %s\n",
			strerror(errno));
		goto fail;
	}

	if (asprintf(&str_uid, "%u", uid) < 0) {
		fprintf(stderr, "lu: can't allocate memory for uid: %s\n",
			strerror(errno));
		goto fail;
	}

	if (strlen(str_uid) != write(fd, str_uid, strlen(str_uid))) {
		fprintf(stderr,
			"lu: failed to write the uid %u: %s\n",
			uid, strerror(errno));
		goto fail;
	}

	rc = 0;

fail:
	if (fd != -1)
		close(fd);

	free(str_uid);

	return rc;
}

static int set_privileges(char *uname, uid_t uid, gid_t gid)
{
	int ngroups = 0, rc = -1;
	gid_t *grps = NULL;

	/* First get the number of groups for uname */
	getgrouplist(uname, gid, grps, &ngroups);
	grps = calloc(ngroups, sizeof(gid_t));
	/* Now get the groups for uname */
	if (getgrouplist(uname, gid, grps, &ngroups) == -1) {
		fprintf(stderr,
			"lu: getgrouplist failed for username %s\n", uname);
		goto fail;
	}

	if (setgroups(ngroups, grps) == -1) {
		fprintf(stderr,
			"lu: failed to set groups for username %s: %s\n",
			uname, strerror(errno));
		goto fail;
	}

	if (setresgid(gid, gid, gid) == -1) {
		fprintf(stderr,
			"lu: failed to set real, effective, and saved group for gid %u: %s\n",
			gid, strerror(errno));
		goto fail;
	}

	if (setresuid(uid, uid, uid) == -1) {
		fprintf(stderr,
			"lu: failed to set real, effective, and saved user for uid %u: %s\n",
			uid, strerror(errno));
		goto fail;
	}

	rc = 0;

fail:
	free(grps);

	return rc;
}

int main(int argc, char **argv)
{
	struct passwd *pwd = NULL;
	char *uname = NULL;
	char *cmdargs[argc];
	int rc = 0, setprivs = 0, opt, option_index = 0, j;

	static const struct option long_options[] = {
		{ "user",	required_argument,	NULL,	'u' },
		{ "setprivs",	no_argument,	NULL,	's' },
		{ "help",	no_argument,	NULL,	'h' },
		{ NULL,	0, NULL, 0 },
	};

	while ((opt = getopt_long_only(argc,
					argv,
					"+u:sh",
					long_options, &option_index)) != -1) {
		switch (opt) {
		case 'u':
			uname = optarg;
			break;

		case 's':
			setprivs = 1;
			break;

		case 'h':
			print_usage();
			return 0;

		case '?':
		default:
			/* Print usage & return, getopt has printed an error */
			print_usage();
			return 0;
		}
	}

	if (uname == NULL) {
		fprintf(stderr, "lu: must supply user to set loginuid to\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (optind >= argc) {
		fprintf(stderr, "lu: must supply command to run\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	/* Copy remaining argv in cmdargs to execute */
	for (j = 0; optind < argc; j++)
		cmdargs[j] = argv[optind++];
	for (; j < argc; j++)
		cmdargs[j] = (char *) NULL;

	pwd = getpwnam(uname);
	if (pwd == NULL) {
		fprintf(stderr,
			"lu: failed to get struct pwd for %s: %s\n",
			uname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (update_loginuid(pwd->pw_uid) < 0) {
		fprintf(stderr,
			"lu: failed to set loginuid for user %s\n", uname);
		exit(EXIT_FAILURE);
	}

	if (setprivs) {
		if (set_privileges(uname, pwd->pw_uid, pwd->pw_gid) < 0) {
			fprintf(stderr,
				"lu: failed to set privileges for uname %s\n",
				uname);
			exit(EXIT_FAILURE);
		}
	}

	rc = execvp(cmdargs[0], cmdargs);

	return rc;
}
