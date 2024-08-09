//
//  Server.c
//  Heavily simplified version of iDownload
//  All credits go to Linus Henze
//
//  Modified by Alfie CG & Ingan121
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

// https://stackoverflow.com/a/36204023
int uptime() {
    struct timeval boottime;
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    size_t size = sizeof(boottime);
    time_t now;
    time_t uptime = -1;

    (void)time(&now);

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec != 0) {
        uptime = now - boottime.tv_sec;
    }
    return uptime;
}

bool readToNewline(FILE *f, char *buf, size_t size) {
    size_t cmdOffset = 0;
    memset(buf, 0, size);
    
    while (1) {
        // Read incoming data
        size_t status = fread(buf + cmdOffset, 1, 1, f);
        if (status < 1) {
            return false;
        }
        
        cmdOffset++;
        if (cmdOffset >= (size-1)) {
            fprintf(f, "\r\nERROR: Command to long!\r\n");
            cmdOffset = 0;
            memset(buf, 0, size);
            continue;
        }
        
        // Check if there is a newline somewhere
        char *loc = strstr(buf, "\n");
        if (!loc) {
            continue;
        }
        
        // There is
        if ((loc - 1) >= buf) {
            if (*(loc - 1) == '\r') {
                loc--;
            }
        }
        
        *loc = 0;
        return true;
    }
}

char *getParameter(char *buf, int param) {
    char *loc = buf;
    
    for (int i = 0; i < param; i++) {
        loc = strstr(loc, " ");
        if (!loc) {
            return NULL;
        }
        
        loc++; // Skip over the space
    }
    
    char *next = strstr(loc, " ");
    if (!next) {
        return strdup(loc);
    }
    
    *next = 0;
    char *data = strdup(loc);
    *next = ' ';
    
    return data;
}

void printInfo(FILE *f, char *file) {
    struct stat fileStat;
    if(lstat(file, &fileStat) < 0) {
        fprintf(f, "info: %s\r\n", strerror(errno));
        return;
    }
    
    fprintf(f, "---------------------------\r\n");
    fprintf(f, "Information for %s\r\n", file);
    fprintf(f, "---------------------------\r\n");
    if (fileStat.st_size != 1) {
        fprintf(f, "File Size: \t\t%lld bytes\r\n", fileStat.st_size);
    } else {
        fprintf(f, "File Size: \t\t1 byte\r\n");
    }
    
    //fprintf(f, "Number of Links: \t%d\r\n", fileStat.st_nlink);
    //fprintf(f, "File inode: \t\t%llu\r\n", fileStat.st_ino);
    
    fprintf(f, "File Permissions: \t");
    fprintf(f, (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
    fprintf(f, (fileStat.st_mode & S_IRUSR) ? "r" : "-");
    fprintf(f, (fileStat.st_mode & S_IWUSR) ? "w" : "-");
    if (fileStat.st_mode & S_ISUID) {
        fprintf(f, (fileStat.st_mode & S_IXUSR) ? "s" : "S");
    } else {
        fprintf(f, (fileStat.st_mode & S_IXUSR) ? "x" : "-");
    }
    fprintf(f, (fileStat.st_mode & S_IRGRP) ? "r" : "-");
    fprintf(f, (fileStat.st_mode & S_IWGRP) ? "w" : "-");
    if (fileStat.st_mode & S_ISGID) {
        fprintf(f, (fileStat.st_mode & S_IXGRP) ? "s" : "S");
    } else {
        fprintf(f, (fileStat.st_mode & S_IXGRP) ? "x" : "-");
    }
    fprintf(f, (fileStat.st_mode & S_IROTH) ? "r" : "-");
    fprintf(f, (fileStat.st_mode & S_IWOTH) ? "w" : "-");
    fprintf(f, (fileStat.st_mode & S_IXOTH) ? "x" : "-");
    fprintf(f, "\r\n");
    fprintf(f, "File Owner UID: \t%d\r\n", fileStat.st_uid);
    fprintf(f, "File Owner GID: \t%d\r\n", fileStat.st_gid);
    
    if (S_ISLNK(fileStat.st_mode)) {
        fprintf(f, "\r\n");
        fprintf(f, "The file is a symbolic link.\r\n");
    }
}

void printHelp(FILE *f) {
    fprintf(f, "The following commands are supported:\r\n");
    fprintf(f, "exit:                     Close connection\r\n");
    fprintf(f, "uname:                    Print device info\r\n");
    fprintf(f, "uptime:                   Print uptime\r\n");
    fprintf(f, "pwd:                      Print current working directory\r\n");
    fprintf(f, "cd <directory>:           Change directory\r\n");
    fprintf(f, "ls <optional path>:       List directory\r\n");
    fprintf(f, "clear:                    Clear screen\r\n");
    fprintf(f, "cat <file>:               Print contents of file\r\n");
    fprintf(f, "rm <file>:                Delete file\r\n");
    fprintf(f, "cp <src> <dst>            Copy a file\r\n");
    fprintf(f, "chmod <mode> <file>:      Chmod a file. Mode must be octal\r\n");
    fprintf(f, "chown <uid> <gid> <file>: Chown a file\r\n");
    fprintf(f, "info <file>:              Print file infos\r\n");
    fprintf(f, "mkdir <name>:             Create a directory\r\n");
    fprintf(f, "help:                     Print this help\r\n");
    fprintf(f, "\r\n");
}

void handleConnection(int socket, int start_uptime) {
    FILE *f = fdopen(socket, "r+b");
    if (f == NULL) {
        return;
    }
    
    fprintf(f, "AlfieShell ready.\n");
    fprintf(f, "Uptime on start: %d\r\n", start_uptime);
    fprintf(f, "Uptime on connection: %d\r\n", uptime());
    
    char *cmdBuffer = malloc(2048);
    
    while (1) {
        fprintf(f, "$ ");
        
        // Read a command
        if (!readToNewline(f, cmdBuffer, 2048)) {
            break;
        }
        
        // Guaranteed to always exist
        char *cmd = getParameter(cmdBuffer, 0);
        
        // Execute it
        if (strcmp(cmd, "help") == 0) {
            printHelp(f);
        } else if (strcmp(cmd, "exit") == 0) {
            fprintf(f, "Bye!\r\n");
            break;
        } else if (strcmp(cmd, "uname") == 0) {
            struct utsname buf;
            if (uname(&buf) < 0) {
                fprintf(f, "uname: %d\r\n", errno);
            }
            fprintf(f, "%s %s %s %s %s\r\n", buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
        } else if (strcmp(cmd, "pwd") == 0) {
            char cwd[PATH_MAX];
            memset(cwd, 0, PATH_MAX);
            getcwd(cwd, PATH_MAX);
            
            fprintf(f, "%s\r\n", cwd);
        } else if (strcmp(cmd, "cd") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (param) {
                if (chdir(param) < 0) {
                    fprintf(f, "cd: %s\r\n", strerror(errno));
                }
                
                free(param);
            } else {
                fprintf(f, "Usage: cd <directory>\r\n");
            }
        } else if (strcmp(cmd, "ls") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (!param) {
                param = strdup(".");
            }
            
            DIR *d;
            struct dirent *dir;
            d = opendir(param);
            if (d) {
                readdir(d); // Skip .
                readdir(d); // Skip ..
                
                while ((dir = readdir(d)) != NULL) {
                    fprintf(f, "%s\r\n", dir->d_name);
                }
                
                closedir(d);
            } else {
                fprintf(f, "ls: %s\r\n", strerror(errno));
            }
            
            free(param);
        } else if (strcmp(cmd, "clear") == 0) {
            fprintf(f, "\E[H\E[J");
        } else if (strcmp(cmd, "cp") == 0) {
            char *src = getParameter(cmdBuffer, 1);
            if (src) {
                char *dst = getParameter(cmdBuffer, 2);
                if (dst) {
                    FILE *srcFile = fopen(src, "rb");
                    if (srcFile) {
                        fseek(srcFile, 0, SEEK_END);
                        size_t size = ftell(srcFile);
                        fseek(srcFile, 0, SEEK_SET);
                        
                        char *buffer = malloc(size + 1);
                        memset(buffer, 0, size + 1);
                        if (buffer) {
                            fread(buffer, size, 1, srcFile);
                            
                            FILE *dstFile = fopen(dst, "wb+");
                            if (dstFile) {
                                fwrite(buffer, size, 1, dstFile);
                                
                                fclose(dstFile);
                            } else {
                                fprintf(f, "cp: dst: %s\r\n", strerror(errno));
                            }
                        } else {
                            fprintf(f, "cp: src: %s\r\n", strerror(errno));
                        }
                        
                        free(buffer);
                        fclose(srcFile);
                    } else {
                        fprintf(f, "cp: src: %s\r\n", strerror(errno));
                    }
                    
                    free(dst);
                } else {
                    fprintf(f, "Usage: cp <src> <dst>\r\n");
                }
                
                free(src);
            } else {
                fprintf(f, "Usage: cp <src> <dst>\r\n");
            }
        } else if (strcmp(cmd, "cat") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (param) {
                FILE *file = fopen(param, "rb");
                if (file) {
                    fseek(file, 0, SEEK_END);
                    size_t size = ftell(file);
                    fseek(file, 0, SEEK_SET);
                    
                    char *buffer = malloc(size + 1);
                    memset(buffer, 0, size + 1);
                    if (buffer) {
                        fread(buffer, size, 1, file);
                        fprintf(f, "%s\r\n", buffer);
                    } else {
                        fprintf(f, "cat: %s\r\n", strerror(errno));
                    }
                    
                    free(buffer);
                    fclose(file);
                } else {
                    fprintf(f, "cat: %s\r\n", strerror(errno));
                }
                
                free(param);
            } else {
                fprintf(f, "Usage: cat <file>\r\n");
            }
        } else if (strcmp(cmd, "rm") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (param) {
                if (unlink(param) < 0) {
                    fprintf(f, "rm: %s\r\n", strerror(errno));
                }
                
                free(param);
            } else {
                fprintf(f, "Usage: rm <file>\r\n");
            }
        } else if (strcmp(cmd, "chmod") == 0) {
            char *modeStr = getParameter(cmdBuffer, 1);
            if (modeStr) {
                char *param = getParameter(cmdBuffer, 2);
                if (param) {
                    size_t mode = strtoul(modeStr, NULL, 8);
                    if (mode) {
                        if (chmod(param, mode) < 0) {
                            fprintf(f, "chmod: %s\r\n", strerror(errno));
                        }
                    } else {
                        fprintf(f, "Usage: chmod <mode, octal> <file>\r\n");
                    }
                    
                    free(param);
                } else {
                    fprintf(f, "Usage: chmod <mode, octal> <file>\r\n");
                }
                
                free(modeStr);
            } else {
                fprintf(f, "Usage: chmod <mode, octal> <file>\r\n");
            }
        } else if (strcmp(cmd, "chown") == 0) {
            char *uid_str = getParameter(cmdBuffer, 1);
            if (uid_str) {
                char *gid_str = getParameter(cmdBuffer, 2);
                if (gid_str) {
                    char *param = getParameter(cmdBuffer, 3);
                    if (param) {
                        size_t uid = strtoul(uid_str, NULL, 0);
                        size_t gid = strtoul(gid_str, NULL, 0);
                        if (chown(param, (uid_t) uid, (gid_t) gid) < 0) {
                            fprintf(f, "chown: %s\r\n", strerror(errno));
                        }
                        
                        free(param);
                    } else {
                        fprintf(f, "Usage: chown <uid> <gid> <file>\r\n");
                    }
                    
                    free(gid_str);
                } else {
                    fprintf(f, "Usage: chown <uid> <gid> <file>\r\n");
                }
                
                free(uid_str);
            } else {
                fprintf(f, "Usage: chown <uid> <gid> <file>\r\n");
            }
        } else if (strcmp(cmd, "info") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (param) {
                printInfo(f, param);
                free(param);
            } else {
                fprintf(f, "Usage: info <file>\r\n");
            }
        } else if (strcmp(cmd, "mkdir") == 0) {
            char *param = getParameter(cmdBuffer, 1);
            if (param) {
                if (mkdir(param, 0777) < 0) {
                    fprintf(f, "mkdir: %s\r\n", strerror(errno));
                }
                
                free(param);
            } else {
                fprintf(f, "Usage: mkdir <name>\r\n");
            }
        } else if (strcmp(cmd, "uptime") == 0) {
            fprintf(f, "%d seconds\r\n", uptime());
        } else if (strcmp(cmd, "") == 0) {
            // do nothing
        } else {
            fprintf(f, "Unknown command: %s\r\n", cmd);
        }
        
        free(cmd);
    }
    
    fclose(f);
}

__attribute__((constructor))
static int dylibMain() {
    int start_uptime = uptime();
    dispatch_async(dispatch_queue_create("Server", NULL), ^{
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            printf("Error!");
            exit(-1);
        }
        
        dup2(server_fd, 10);
        close(server_fd);
        server_fd = 10;
        
        int option = 1;
        
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)) < 0) {
            printf("Error!");
            exit(-1);
        }
        
        struct sockaddr_in server;
        
        memset(&server, 0, sizeof (server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr("127.0.0.1");
        server.sin_port = htons(1338);
        
        if (bind(server_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
            printf("Port 1338 already occupied!");
            exit(-1);
        }
        
        if (listen(server_fd, SOMAXCONN) < 0) {
            exit(-1);
        }
        
        while (1) {
            int new_socket = accept(server_fd, NULL, NULL);
            if (new_socket == -1) {
                if (errno == EINTR) {
                    continue;
                }
                
                printf("Error!\n"); //exit(-1);
            }
            
            dispatch_async(dispatch_queue_create(NULL, NULL), ^{
                handleConnection(new_socket, start_uptime);
            });
        }
    });
    return 0;
}
