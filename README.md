# Webserv (42 Project)

This project is an implementation of an HTTP/1.1 server based on the 42 school curriculum.

## Features

*   Parses an NGINX-like configuration file.
*   Listens on multiple ports/hosts.
*   Handles GET, POST, DELETE HTTP methods.
*   Serves static files.
*   Handles directory listing.
*   Supports file uploads.
*   Executes CGI scripts (e.g., PHP, Python).
*   Uses non-blocking I/O with `poll` (or equivalent).
*   Handles basic error pages.

## Build

```bash
make
```

## Run

```bash
./webserv [path/to/your_config.conf]
```

If no configuration file is provided, it will attempt to use `default.conf` in the current directory.

## Configuration

See `default.conf` for an example configuration file structure.

Key directives include:

*   `server`: Defines a virtual server.
    *   `listen [host:]port;`: Specifies the address and port to listen on.
    *   `server_name name1 name2 ...;`: Sets server names.
    *   `error_page code ... /path/to/error.html;`: Defines custom error pages.
    *   `client_max_body_size size;`: Sets the maximum allowed request body size (e.g., `10m`).
*   `location path { ... }`: Defines rules for specific URI paths.
    *   `root /path/to/document/root;`: Sets the document root for requests.
    *   `index file1 file2 ...;`: Specifies default files to serve for directory requests.
    *   `limit_except method1 method2 ...;` or `allow_methods method1 ...;`: Restricts allowed HTTP methods.
    *   `autoindex on | off;`: Enables/disables directory listing.
    *   `return code [URL];`: Performs an HTTP redirect.
    *   `cgi_pass path/to/interpreter;`: Specifies the CGI interpreter for specific extensions (often paired with a file extension match in the location path, e.g., `location ~ \.php$`).
    *   `cgi_param PARAM value;`: Sets environment variables for CGI.
    *   `upload_store /path/to/save/uploads;`: Defines the directory to save uploaded files. 