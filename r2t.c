#include "r2t.h"

const char name[] = "rsstail " VERSION ", (C) 2006-2016 by folkert@vanheusden.com";

void replace(char *const in, const char *const what, char by_what)
{
	size_t what_len = strlen(what);

	/* replace &lt; etc. */
	for(;;)
	{
		char *str = strstr(in, what);
		if (!str)
			break;

		memcpy(str + 1, str + what_len, strlen(str + what_len) + 1);
		*str = by_what;
	}
}

char *remove_html_tags(const char *const in)
{
	char *copy = strdup(in);

	/* strip <...> */
	for(;;)
	{
		char *lt = strchr(copy, '<'), *gt;
		if (!lt)
			break;

		gt = strchr(lt, '>');
		if (!gt)
			break;

		memcpy(lt, gt + 1, strlen(gt + 1) + 1);
	}

	replace(copy, "&lt;", '<');
	replace(copy, "&gt;", '>');
	replace(copy, "&amp;", '&');

	return copy;
}

/* 0: is a new record, -1: not a new record */
int is_new_record(mrss_item_t *check_list, mrss_item_t *cur_item)
{
	while(check_list)
	{
		if (check_list -> pubDate != NULL && cur_item -> pubDate != NULL)
		{
			if (strcmp(check_list -> pubDate, cur_item -> pubDate) == 0)
				return -1;
		}
		else
		{
			int navail = 0, nequal = 0;

			if (check_list -> title != NULL && cur_item -> title != NULL)
			{
				navail++;

				if (strcmp(check_list -> title, cur_item -> title) == 0)
					nequal++;
			}
			if (check_list -> link  != NULL && cur_item -> link  != NULL)
			{
				navail++;

				if (strcmp(check_list -> link, cur_item -> link ) == 0)
					nequal++;
			}
			if (check_list -> description != NULL && cur_item -> description != NULL)
			{
				navail++;

				if (strcmp(check_list -> description, cur_item -> description) == 0)
					nequal++;
			}

			if (navail == nequal && navail > 0)
			{
				return -1;
			}
		}

		check_list = check_list -> next;
	}

	return 0;
}

void version(void)
{
	printf("%s\n", name);
}

char* my_convert(iconv_t converter, const char *input)
{
	size_t in_size = strlen(input);
	size_t out_size = (in_size + 1) * 6; // seems to be enough

	char *output_start = (char *)calloc(1, out_size), *output = output_start;
	if (!output_start)
		return NULL;

	do
	{
		size_t converted = iconv(converter, (char **) &input, &in_size, &output, &out_size);

		if (converted == (size_t) -1)
		{
			free (output_start);
			return NULL;
		}
	}
	while(in_size);

	return output_start;
}

void usage(void)
{
	version();

	printf("-t	show a timestamp of when the item was processed\n");
	printf("-l	show item's link\n");
	printf("-e	show item's enclosure URL\n");
	printf("-d	show item's description\n");
	printf("-p	show item's publication date\n");
	printf("-a	show item's author\n");
	printf("-c	show item's comments\n");
	printf("-g	show item's GUID\n");
	printf("-N	do not show headings\n");
	printf("-b x	limit description/comments to x bytes\n");
	printf("-z	continue even if there are XML parser errors in the RSS feed\n");
	printf("-Z x	print string 'x' before headings\n");
	printf("-n x	initially show only first 'x' items\n");
	printf("-r	reverse output, so it looks more like an RSS feed\n");
	printf("-H	strip HTML tags\n");
	/*	printf("-o x    only show items newer then x[s/M/h/d/m/y]\n");	*/
	printf("-A x	authenticate against webserver (username:password)\n");
	printf("-u url	URL of RSS feed to tail\n");
	printf("-i x	check interval in seconds (default is 15min)\n");
	printf("-x x	proxy server to use (host[:port])\n");
	printf("-y x	proxy authentication (username:password)\n");
	printf("-P	do not exit when an error occurs\n");
	printf("-1	one shot mode: print new items and exit\n");
	printf("-v	be verbose (repeat to increase verbosity)\n");
	printf("-V	show version and exit\n");
	printf("-h	this help\n");
}

int main(int argc, char *argv[])
{
	char **url = NULL;
	size_t n_url = 0;
	size_t cur_url = 0;
	int check_interval = 15 * 60;
	mrss_t **data_prev = NULL;
	mrss_t **data_cur = NULL;
	char *proxy = NULL, *proxy_auth = NULL;
	int sw = 0;
	int verbose = 0;
	char show_timestamp = 0, show_link = 0, show_enclosure_url = 0, show_description = 0, show_pubdate = 0, show_author = 0, show_comments = 0, show_guid = 0;
	char show_title = 1;
	char strip_html = 0, no_error_exit = 0;
	char one_shot = 0;
	char no_heading = 0;
	size_t bytes_limit = 0;
	time_t last_changed = (time_t)0;
	char continue_on_error = 0;
	int show_n = -1;
	char *heading = NULL;
	mrss_options_t mot;
	char *auth = NULL;
	char *current_encoding = NULL;
	char reverse = 0;
	iconv_t converter = 0;

	memset(&mot, 0x00, sizeof(mot));

	while((sw = getopt(argc, argv, "A:Z:1b:PHztTledrpacgu:Ni:n:x:y:vVh")) != -1)
	{
		switch(sw)
		{
			case 'A':
				auth = optarg;
				break;

			case 'Z':
				heading = optarg;
				break;

			case 'N':
				no_heading = 1;
				break;

			case '1':
				one_shot = 1;
				break;

			case 'b':
				bytes_limit = (size_t) atoi(optarg);
				if (bytes_limit <= 0)
				{
					printf("-b requires a number > 0\n");
					return 1;
				}
				break;

			case 'P':
				no_error_exit = 1;
				break;

			case 'H':
				strip_html = 1;
				break;

			case 'n':
				show_n = atoi(optarg);
				if (show_n < 0)
				{
					printf("-n requires an positive value\n");
					return 1;
				}
				else if (show_n > 50)
					printf("Initially showing more then 50 items, must be one hell of an rss feed!\n");
				break;

#if 0
			case 'o':
				dummy = optarg[strlen(optarg) - 1];
				max_age = atoi(optarg);
				if (max_age < 0)
				{
					printf("-o requires an positive value\n");
					return 1;
				}
				if (dummy == 's')
					max_age *= 1;
				else if (dummy == 'M')
					max_age *= 60;
				else if (dummy == 'h')
					max_age *= 3600;
				else if (dummy == 'd')
					max_age *= 86400;
				else if (dummy == 'm')
					max_age *= 86400 * 31;
				else if (dummy == 'y')
					max_age *= 86400 * 365.25;
				else if (isalpha(dummy))
				{
					printf("'%c' is a not recognized multiplier\n", dummy);
					return 1;
				}
				break;
#endif

			case 'z':
				continue_on_error = 1;
				break;

			case 'T':
				show_title = 0;
				break;

			case 't':
				show_timestamp = 1;
				break;

			case 'l':
				show_link = 1;
				break;

			case 'e':
				show_enclosure_url = 1;
				break;

			case 'd':
				show_description = 1;
				break;

			case 'r':
				reverse = 1;
				break;

			case 'p':
				show_pubdate = 1;
				break;

			case 'a':
				show_author = 1;
				break;

			case 'c':
				show_comments = 1;
				break;

			case 'g':
				show_guid = 1;
				break;

			case 'u':
				url = (char **)realloc(url, sizeof(char *) * (n_url + 1));
				if (!url)
				{
					fprintf(stderr, "Cannot allocate memory\n");
					return 2;
				}

				url[n_url++] = optarg;

				break;

			case 'i':
				check_interval = atoi(optarg);
				break;

			case 'x':
				proxy = optarg;
				break;

			case 'y':
				proxy_auth = optarg;
				break;

			case 'v':
				verbose++;
				break;

			case 'V':
				version();
				return 1;

			case 'h':
			default:
				usage();
				return 1;
		}
	}

	mot.timeout = check_interval;
	mot.proxy = proxy;
	mot.proxy_authentication = proxy_auth;
	mot.user_agent = (char *)name;
	mot.authentication = auth;

	if (n_url == 0)
	{
		fprintf(stderr, "Please give the URL of the RSS feed to check with the '-u' parameter.\n");
		return 1;
	}

	data_prev = (mrss_t **)calloc(n_url, sizeof(mrss_t *));
	data_cur  = (mrss_t **)calloc(n_url, sizeof(mrss_t *));
	if (!data_prev || !data_cur)
	{
		fprintf(stderr, "Cannot allocate memory\n");
		return 2;
	}

	setlocale(LC_ALL, "");
	current_encoding = nl_langinfo(CODESET);

	if (verbose)
	{
		printf("Monitoring RSS feeds:\n");

		for(size_t loop=0; loop<n_url; loop++)
			printf("\t%s\n", url[loop]);

		printf("Check interval: %d\n", check_interval);
		printf("Output current_encoding: %s\n", current_encoding);
	}

	for(;;)
	{
		mrss_error_t err_read;
		mrss_item_t *item_cur = NULL;
		mrss_item_t *first_item[n_url];
		mrss_item_t *tmp_first_item;
		time_t cur_last_changed;
		int n_shown = 0;

		if (verbose)
			printf("Retrieving RSS feed '%s'...\n", url[cur_url]);

		if ((err_read = mrss_get_last_modified_with_options(url[cur_url], &cur_last_changed, &mot)) != MRSS_OK)
		{
			if (err_read == MRSS_ERR_POSIX)
			{
				if (errno == EINPROGRESS)
				{
					fprintf(stderr, "Time-out while connecting to RSS feed, continuing\n");
					goto goto_next_url;
				}
			}

			fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));

			if (no_error_exit)
				goto goto_next_url;

			return 2;
		}

		if (cur_last_changed == last_changed && cur_last_changed != 0)
		{
			if (verbose)
				printf("Feed did not change since last check.\n");

			goto goto_next_url;
		}

		if (verbose > 2)
			printf("Feed change detected, %s", ctime(&cur_last_changed));

		last_changed = cur_last_changed;

		if ((err_read = mrss_parse_url_with_options(url[cur_url], &data_cur[cur_url], &mot)) != MRSS_OK)
		{
			if (err_read == MRSS_ERR_POSIX)
			{
				if (errno == EINPROGRESS)
				{
					fprintf(stderr, "Time-out while connecting to RSS feed, continuing\n");
					goto goto_next_url;
				}
			}
			else if (err_read == MRSS_ERR_PARSER && continue_on_error)
			{
				fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));
				goto goto_next_url;
			}

			fprintf(stderr, "Error reading RSS feed: %s\n", mrss_strerror(err_read));

			if (no_error_exit)
				goto goto_next_url;

			return 2;
		}

		if (data_cur[cur_url]->encoding == NULL)
			data_cur[cur_url]->encoding = strdup("utf-8");

		if (verbose)
			printf("Creating converter %s -> %s\n", data_cur[cur_url] -> encoding, current_encoding);

		converter = iconv_open(current_encoding, data_cur[cur_url] -> encoding);

		if (converter == (iconv_t) -1)
		{
			fprintf(stderr, "Error creating converter: %s \n", strerror(errno));
			return 2;
		}

		item_cur = data_cur[cur_url] -> item;

		if (reverse)
		{
			if (verbose)
				printf("Reversing...\n");

			mrss_item_t *rev_item_cur = NULL;
			mrss_item_t *rev_item_last = NULL;

			for (;;)
			{
				rev_item_last = rev_item_cur;
				rev_item_cur = item_cur;

				if (!item_cur)
					break;

				if (!item_cur -> next)
				{
					rev_item_cur -> next = rev_item_last;
					break;
				}

				item_cur = item_cur -> next;
				rev_item_cur -> next = rev_item_last;
			}
		}

		tmp_first_item = item_cur;

		while(item_cur)
		{
			if ((data_prev[cur_url] && is_new_record(first_item[cur_url], item_cur) != -1) || !data_prev[cur_url])
			{
				if (!data_prev[cur_url] && n_shown >= show_n && show_n != -1)
				{
					item_cur = item_cur -> next;
					continue;
				}

				n_shown++;

				if (show_link + show_enclosure_url + show_description + show_pubdate + show_author + show_comments > 1)
					printf("\n");

				if (show_timestamp)
				{
					time_t now = time(NULL);
					struct tm *now_tm = localtime(&now);

					printf("%04d/%02d/%02d %02d:%02d:%02d  ",
							now_tm -> tm_year + 1900,
							now_tm -> tm_mon + 1,
							now_tm -> tm_mday,
							now_tm -> tm_hour,
							now_tm -> tm_min,
							now_tm -> tm_sec);
				}

				if (heading)
					printf(" %s", heading);

				if (show_title && item_cur -> title != NULL)
				{
					char *title = my_convert(converter, item_cur -> title);

					if (title)
					{
						printf("%s%s\n", no_heading?" ":"Title: ", title);
						free(title);
					}
				}

				if (show_link && item_cur -> link != NULL)
				{
					char *link = my_convert(converter, item_cur -> link);

					if (link)
					{
						printf("%s%s\n", no_heading?" ":"Link: ", item_cur -> link);
						free(link);
					}
				}

				if (show_enclosure_url && item_cur -> enclosure_url != NULL)
				{
					char *enclosure_url = my_convert (converter, item_cur -> enclosure_url);

					if (enclosure_url)
					{
						printf("%s%s\n", no_heading?" ":"Enclosure URL: ", enclosure_url);
						free(enclosure_url);
					}
				}

				if (show_description && item_cur -> description != NULL)
				{
					if (strip_html)
					{
						char *stripped = remove_html_tags(item_cur -> description);

						if (bytes_limit != 0 && bytes_limit < strlen(stripped))
							stripped[bytes_limit] = 0x00;

						char *description = my_convert(converter, stripped);
						if (description)
						{
							printf("%s%s\n", no_heading?" ":"Description: ", description);
							free(description);
						}

						free(stripped);
					}
					else
					{
						if (bytes_limit != 0 && bytes_limit < strlen(item_cur -> description))
							(item_cur -> description)[bytes_limit] = 0x00;

						char *description = my_convert(converter, item_cur -> description);
						if (description)
						{
							printf("%s%s\n", no_heading?" ":"Description: ", description);
							free(description);
						}
					}
				}

				if (show_pubdate && item_cur -> pubDate != NULL)
					printf("%s%s\n", no_heading?" ":"Pub.date: ", item_cur -> pubDate);

				if (show_author && item_cur -> author != NULL){
					char *author = my_convert(converter, item_cur -> author);
					if (author)
					{
						printf("%s%s\n", no_heading?" ":"Author: ", author);
						free(author);
					}
				}

				if (show_comments && item_cur -> comments != NULL)
				{
					if (bytes_limit != 0 && bytes_limit < strlen(item_cur -> comments))
						(item_cur -> comments)[bytes_limit] = 0x00;

					char *comments = my_convert(converter, item_cur -> comments);
					if (comments)
					{
						printf("%s%s\n", no_heading?" ":"Comments: ", item_cur -> comments);
						free(comments);
					}
				}

				if (show_guid && item_cur -> guid != NULL)
					printf("%s%s\n", no_heading?" ":"guid: ", item_cur -> guid);
			}

			item_cur = item_cur -> next;
		}

		if (data_prev[cur_url])
		{
			mrss_error_t err_free = mrss_free(data_prev[cur_url]);

			if (err_free != MRSS_OK)
			{
				fprintf(stderr, "Error freeing up memory: %s\n", mrss_strerror(err_free));

				if (no_error_exit)
					goto goto_next_url;

				return 2;
			}
		}

		data_prev[cur_url] = data_cur[cur_url];
		data_cur[cur_url] = NULL;
		first_item[cur_url] = tmp_first_item;

goto_next_url:
		if (converter)
		{
			iconv_close(converter);
			converter = 0;
		}

		cur_url++;
		if (cur_url >= n_url)
			cur_url = 0;

		fflush(stdout);

		if (one_shot)
			break;

		if (verbose > 2)
			printf("Sleeping...\n");

		sleep((unsigned int)check_interval / (unsigned int) n_url);
	}

	return 0;
}
