HANDLE server_process = 0;

struct Dynamic_String {
    char *buffer;
    size_t length, capacity;
};

Dynamic_String dynamic_string_make(size_t initial_size) {
    Dynamic_String result = {};

    result.capacity = initial_size;
    result.length = 0;
    result.buffer = (char *)calloc(initial_size, sizeof(result.buffer[0]));

    return result;
}

void dynamic_string_concatenate(Dynamic_String *destination,
                                const char *source)
{
    size_t source_length = strlen(source);

    // Reallocate if the size is exceeded
    while (destination->length + source_length > destination->capacity) {
        destination->capacity *= 2;
    }
    destination->buffer = (char *)realloc(destination->buffer, destination->capacity);

    memset(destination->buffer + destination->length, 0, destination->capacity - destination->length);

    for (size_t i = 0; i < source_length; i++) {
        size_t index = destination->length + i;
        destination->buffer[index] = source[i];
    }
    destination->length += source_length;
    destination->buffer[destination->length] = 0;
}

// Uses the selected links and makes an HTML file with those links
// with the filepath
void StartServer(const char *server_path) {
    char html_filepath[MAX_PATH] = {};
    strcat(html_filepath, server_path);
    strcat(html_filepath, "index.html");

    if (!PathFileExistsA(server_path)) {
        CreateDirectory(server_path, 0);
    }

    FILE *fp = fopen(html_filepath, "w");

    assert(fp);

    Dynamic_String string = dynamic_string_make(64);

    char *ip_address = 0;

    // Magic to get the local IP Address
    { 
        WSADATA wsaData;
        char hostname[256];
        struct hostent *host_entry;
        char *IPbuffer;

        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
            return;
        }

        // Get the host name
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
            printf("gethostname failed. Error Code: %d\n", WSAGetLastError());
            WSACleanup();
            return;
        }

        // Get the host information
        if ((host_entry = gethostbyname(hostname)) == NULL) {
            printf("gethostbyname failed. Error Code: %d\n", WSAGetLastError());
            WSACleanup();
            return;
        }

        // Convert the address to a string
        IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

        ip_address = IPbuffer;

        // Cleanup Winsock
        WSACleanup();
    }

    dynamic_string_concatenate(&string, "<html>\n<body>\n");

    char url_line[2048] = {};
    sprintf(url_line, "http://%s:6942", ip_address);

    dynamic_string_concatenate(&string, "Access this on your device via this link: <a href=\"");
    dynamic_string_concatenate(&string, url_line);
    dynamic_string_concatenate(&string, "\">");
    dynamic_string_concatenate(&string, url_line);
    dynamic_string_concatenate(&string, "</a>");

    if (*CustomField(CustomOption::TitleText).input) {
        dynamic_string_concatenate(&string, "<h1>");
        dynamic_string_concatenate(&string, CustomField(CustomOption::TitleText).input);
        dynamic_string_concatenate(&string, "</h1>");
    }

    for (int i = 0; i < selection.link_count; i++) {
        char *description = selection.links[i]->description;
        char *url = selection.links[i]->link;

        if (url[0]) {
            dynamic_string_concatenate(&string, "<a href=\"");
            dynamic_string_concatenate(&string, url);
            dynamic_string_concatenate(&string, "\">");
            dynamic_string_concatenate(&string, url);
            dynamic_string_concatenate(&string, "</a>\n");
        }

        if (description[0]) {
            dynamic_string_concatenate(&string, description);
        }
        dynamic_string_concatenate(&string, "<br>\n");
    }

    dynamic_string_concatenate(&string, "</body>\n</html>");

    fprintf(fp, "%s", string.buffer);

    fclose(fp);

    // Start the python server if it's not on already

    if (server_process == 0) {
        STARTUPINFOA si = {};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi = {};

        char server_working_directory[MAX_PATH] = {};
        strcpy(server_working_directory, server_path);

        char command_line[] = "py -m http.server 6942";
        int ok = CreateProcessA(NULL,
                                command_line,
                                NULL,
                                NULL,
                                false,
                                CREATE_NO_WINDOW,
                                NULL,
                                server_working_directory,
                                &si,
                                &pi);
        server_process = pi.hProcess;

        if (!ok) {
            MessageBoxA(0, "Goodies expects python to be in the PATH as \"py\"", "Couldn't find python", MB_OK | MB_ICONERROR);
            return;
        }
    }

    Link link = {};
    strcpy(link.link, url_line);
    OpenLink(link, false);
}
