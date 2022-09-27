#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

// --------

#define DISPLAY_BUF_INITIAL_NUM_BLOCKS 8

// --------

struct Resources {
	DIR* directory;
	struct dirent* display_buffer;
};

// --------

typedef int (*Selector)(struct dirent*);
typedef void (*Printer)(struct dirent*);
typedef void (*Handler)(struct dirent*);

// --------

// Function declarations:

static void free_resources();
static void free_and_exit(int status_code);

static void parse_args(int argc, char* const argv[]);
static void print_usage_msg(FILE* output_stream, char* const argv[]);
static void fail_with_usage_msg(char* const argv[]);
static void finish_args(int argc, char* const argv[]);

static DIR* open_directory(const char* dir_name);
static void close_directory(DIR* directory);

static void read_entries(DIR* directory);

static int gets_selected(struct dirent* entry);
static void add_selection_filter(Selector filter);
static int not_starting_with_dot(struct dirent* entry);
static int no_dot_dir(struct dirent* entry);

static void print_name(struct dirent* entry);

static void add_to_display_buffer(struct dirent* entry);
static void init_display_buffer();
static void redouble_display_buffer();
static void display_buffered_entries();
static int compare_entries(const void* first, const void* second);

// --------

static const char CWD_SHORT[] = ".";

static struct Resources g_resources = {NULL, NULL};

static const char* g_directory_name = CWD_SHORT;
static int opt_show_dot_dirs = 0;
static int opt_no_dotfiles = 0;
static int opt_unordered = 0;

static Selector g_selection_filters[2] = {NULL, NULL};
static size_t g_num_selection_filters = 0;

static Printer g_printer = print_name;
static Handler g_handler = add_to_display_buffer;

static struct dirent* g_display_buffer = NULL;
static size_t g_display_buffer_size = 0;
static size_t g_num_entries_in_buffer = 0;

// --------

static void free_resources() {
	if(g_resources.directory != NULL) {
		closedir(g_resources.directory);
	}
	if(g_resources.display_buffer != NULL) {
		free(g_resources.display_buffer);
	}
	return;
}

static void free_and_exit(int status_code) {
	free_resources();
	exit(status_code);
}

static void parse_args(int argc, char* const argv[]) {

	int opt;

	while((opt = getopt(argc, argv, "ahnu")) != -1) {

		switch(opt) {
			case 'a':
				opt_show_dot_dirs = 1;
				break;
			case 'h':
				print_usage_msg(stdout, argv);
				exit(EXIT_SUCCESS);
				break;
			case 'n':
				opt_no_dotfiles = 1;
				break;
			case 'u':
				opt_unordered = 1;
				break;
			default:
				fail_with_usage_msg(argv);
				break;
		}
	}

	finish_args(argc, argv);

	return;
}

static void print_usage_msg(FILE* output_stream, char* const argv[]) {
	fprintf(output_stream, "Usage: %s [-ahnu] [directory_name]\n", argv[0]);
	return;
}

static void fail_with_usage_msg(char* const argv[]) {
	print_usage_msg(stderr, argv);
	exit(EXIT_FAILURE);
}

static void finish_args(int argc, char* const argv[]) {

	// If directory name is given, use that instead of ".":
	if(optind == argc - 1) {
		g_directory_name = argv[optind];
	}

	// In case of multiple directory names, fail:
	else if(optind < argc) {
		fail_with_usage_msg(argv);
	}

	if(!opt_show_dot_dirs) {
		add_selection_filter(no_dot_dir);
	}

	if(opt_no_dotfiles) {
		add_selection_filter(not_starting_with_dot);
	}

	if(opt_unordered) {
		// No need to buffer and sort the results, just print them immediately:
		g_handler = g_printer;
	}

	return;
}

static DIR* open_directory(const char* dir_name) {
	DIR* directory = opendir(dir_name);
	g_resources.directory = directory;
	if(directory == NULL) {
		int errnum = errno;
		errno = 0;
		fprintf(stderr, "%s\n", strerror(errnum));
		free_and_exit(EXIT_FAILURE);
	}
	return directory;
}

static void close_directory(DIR* directory) {
	closedir(directory);
	g_resources.directory = NULL;
	return;
}

static void read_entries(DIR* directory) {

	struct dirent* entry;

	errno = 0;
	entry = readdir(directory);

	while(entry != NULL) {
		if(gets_selected(entry)) {
			g_handler(entry);
		}
		errno = 0;
		entry = readdir(directory);
	}

	return;
}

static int gets_selected(struct dirent* entry) {
	for(size_t i = 0; i < g_num_selection_filters; i++)  {
		Selector filter = g_selection_filters[i];
		if(filter != NULL) {
			if(!filter(entry)) {
				return 0;
			}
		}
	}
	return 1;
}

static void add_selection_filter(Selector filter) {
	g_selection_filters[g_num_selection_filters] = filter;
	g_num_selection_filters++;
	return;
}

static int not_starting_with_dot(struct dirent* entry) {
	return ((entry->d_name)[0] != '.');
}

static int no_dot_dir(struct dirent* entry) {
	if(not_starting_with_dot(entry)) {
		return 1;
	}
	switch((entry->d_name)[1]) {
		case '\0':
			return 0;
			break;
		case '.':
			return ((entry->d_name)[2] != '\0');
			break;
		default:
			return 1;
			break;
	}
}

static void print_name(struct dirent* entry) {
	printf("%s\n", entry->d_name);
	return;
}

static void add_to_display_buffer(struct dirent* entry) {

	if(g_display_buffer == NULL) {
		init_display_buffer();
	}

	else if(g_num_entries_in_buffer == g_display_buffer_size) {
		redouble_display_buffer();
	}

	memcpy(g_display_buffer + g_num_entries_in_buffer, entry, sizeof(struct dirent));
	g_num_entries_in_buffer++;

	return;
}

static void init_display_buffer() {

	g_display_buffer = calloc(DISPLAY_BUF_INITIAL_NUM_BLOCKS, sizeof(struct dirent));
	if(g_display_buffer == NULL) {
		perror("calloc");
		free_and_exit(EXIT_FAILURE);
	}

	// Update buffer size:
	g_display_buffer_size = DISPLAY_BUF_INITIAL_NUM_BLOCKS;

	// Add to resources, so we remember to free it later:
	g_resources.display_buffer = g_display_buffer;

	return;
}

static void redouble_display_buffer() {

	size_t new_buffer_size = 2 * g_display_buffer_size;
	// Check for overflow:
	if(new_buffer_size <= g_display_buffer_size) {
		fputs("add_to_display_buffer: overflow\n", stderr);
		free_and_exit(EXIT_FAILURE);
	}

	// Re-allocate:
	struct dirent* new_buffer = reallocarray(g_display_buffer, new_buffer_size, sizeof(struct dirent));
	if(new_buffer == NULL) {
		perror("reallocarray");
		free_and_exit(EXIT_FAILURE);
	}
	g_display_buffer = new_buffer;

	// Update buffer size:
	g_display_buffer_size = new_buffer_size;

	// Update resources:
	g_resources.display_buffer = g_display_buffer;

	return;
}

static void display_buffered_entries() {

	if(g_display_buffer != NULL) {

		// Sort:
		qsort(g_display_buffer, g_num_entries_in_buffer, sizeof(struct dirent), compare_entries);

		// Print:
		for(size_t i = 0; i < g_num_entries_in_buffer; i++)  {
			g_printer(g_display_buffer + i);
		}
	}

	return;
}

static int compare_entries(const void* first, const void* second) {
	return strcasecmp(((struct dirent*) first)->d_name, ((struct dirent*) second)->d_name);
}

// --------

int main(int argc, char** argv) {

	parse_args(argc, argv);

	DIR* directory = open_directory(g_directory_name);

	read_entries(directory);

	close_directory(directory);

	display_buffered_entries();

	free_and_exit(EXIT_SUCCESS);
}


