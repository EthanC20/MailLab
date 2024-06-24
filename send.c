#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>
#include "base64_utils.h"

#define MAX_SIZE 4095

char buf[MAX_SIZE+1];

// 为了方便后面的信息发送等操作
// 以下封装一些socket的函数

#define swap16(x) ((((x)&0xFF) << 8) | (((x) >> 8) & 0xFF))

int custom_send(int sockfd, void *buf, int len, int flag, char *error_msg){
    int send_len = send(sockfd, buf, len, flag);
    if (send_len == -1){
        perror(error_msg);
        exit(EXIT_FAILURE);
    }
    printf("%s\r\n", (char *)buf);

    return send_len;
}


int custom_recv(int sockfd, void *buf, int len, int flag, char *error_msg){
    int recv_len = recv(sockfd, buf, len, 0);
    if (recv_len == -1){
        perror(error_msg);
        exit(EXIT_FAILURE);
    }
    char *temp_buf = (char *)buf;
    temp_buf[recv_len] = '\0';
    printf("%s\r\n", temp_buf);

    return recv_len;
}


char *file2str(const char *path){
    FILE *fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    int fplen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *content = (char *)malloc(fplen);
    fread(content, 1, fplen, fp);
    fclose(fp);
    return content;
}


int gen_base64_file(const char *path){
    FILE *fp = fopen(path, "r");
    if (fp == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
        return -1;
    }

    FILE *fp_base64 = fopen("attach_base64", "w");
    encode_file(fp, fp_base64);
    fclose(fp);
    fclose(fp_base64);
    return 0;
}


// receiver: mail address of the recipient
// subject: mail subject
// msg: content of mail body or path to the file containing mail body
// att_path: path to the attachment
void send_mail(const char* receiver, const char* subject, const char* msg, const char* att_path)
{
    const char* end_msg = "\r\n.\r\n";
    const char* host_name = "smtp.163.com"; // TODO: Specify the mail server domain name
    const unsigned short port = 25; // SMTP server port
    const char* user = "ethanchen20@163.com"; // TODO: Specify the user
    const char* pass = ""; // TODO: Specify the password
    const char* from = "ethanchen20@163.com"; // TODO: Specify the mail address of the sender
    char dest_ip[16]; // Mail server IP address
    int s_fd; // socket file descriptor
    struct hostent *host;
    struct in_addr **addr_list;
    int i = 0;
    int r_size;

    // Get IP from domain name
    if ((host = gethostbyname(host_name)) == NULL){
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    addr_list = (struct in_addr **) host->h_addr_list;
    while (addr_list[i] != NULL)
        ++i;
    strcpy(dest_ip, inet_ntoa(*addr_list[i-1]));

    // TODO: Create a socket, return the file descriptor to s_fd, and establish a TCP connection to the mail server
    // Create a socket
    if ((s_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket file descriptor");
        exit(EXIT_FAILURE);
    }

    // Define the server address
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = (struct in_addr){inet_addr(dest_ip)};
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    socklen_t addr_len = sizeof(server_addr); // addrlen 参数是套接字地址结构（sockaddr）的长度

    // Connect to the server
    if (connect(s_fd, (struct sockaddr*)&server_addr, addr_len) == -1){
        perror("connect");
        close(s_fd);
        exit(EXIT_FAILURE);
    }


    // Print welcome message
    // there is a welcome message from the server using the customized recv function above
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv");

    // Send EHLO command and print server response
    const char* EHLO = "EHLO 163.com\r\n"; // TODO: Enter EHLO command here
    custom_send(s_fd, (void *)EHLO, strlen(EHLO), 0, "send EHLO");
    
    // TODO: Print server response to EHLO command
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv EHLO");

    // TODO: Authentication. Server response should be printed out.
    const char* AUTH_LOGIN = "AUTH login\r\n";
    custom_send(s_fd, (void *)AUTH_LOGIN, strlen(AUTH_LOGIN), 0, "send AUTH");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv AUTH");
    // send username
    char *user_base64 = encode_str(user);
    custom_send(s_fd, (void *)user_base64, strlen(user_base64), 0, "send user");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv user");
    free(user_base64);
    // send password
    char *pass_base64 = encode_str(pass);
    strcat(pass_base64, "\r\n");
    custom_send(s_fd, (void *)pass_base64, strlen(pass_base64), 0, "send password");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv password");
    free(pass_base64);

    // TODO: Send MAIL FROM command and print server response
    sprintf(buf, "MAIL FROM: <%s>\r\n", from);
    custom_send(s_fd, (void *)buf, strlen(buf), 0, "send MAIL FROM");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv MAIL FROM");

    // TODO: Send RCPT TO command and print server response
    sprintf(buf, "RCPT TO: <%s>\r\n", receiver);
    custom_send(s_fd, (void *)buf, strlen(buf), 0, "send RCPT TO");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv RCPT TO");

    // TODO: Send DATA command and print server response
    const char* DATA = "DATA\r\n";
    custom_send(s_fd, (void *)DATA, strlen(DATA), 0, "send DATA");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv DATA");

    // TODO: Send message data
    // To: receiver
    // Cc: (optional)
    // From: sender
    // Subject: subject

    // MIME multipart set boundary string
    const char* boundary = "----=_NextPart_000_0012_01D6D3A1.3F3A3A30";
    // message header
    sprintf(buf,
            "From: %s\r\n"
            "To: %s\r\n"
            "Content-Type: multipart/mixed; boundary=\"%s\"\r\n",
            from, receiver, boundary);
    
    if (subject != NULL){
        strcat(buf, "Subject: ");
        strcat(buf, subject);
        strcat(buf, "\r\n\r\n");
    }
    custom_send(s_fd, (void *)buf, strlen(buf), 0, "send DATA header");

    // message body
    if (msg != NULL){
        sprintf(
            buf,
            "--%s\r\n"
            "Content-Type: text/plain; charset=\"utf-8\"\r\n"
            "Content-Transfer-Encoding: base64\r\n\r\n",
            boundary
        );
        custom_send(s_fd, (void *)buf, strlen(buf), 0, "send DATA msg");

        if (access(msg, F_OK) == 0){
            char *msg_base64 = file2str(msg);
            custom_send(s_fd, (void *)msg_base64, strlen(msg_base64), 0, "send DATA msg file base64");
            free(msg_base64);
        }
        else{
            custom_send(s_fd, (void *)msg, strlen(msg), 0, "send DATA msg text");
        }
        custom_send(s_fd, "\r\n", 2, 0, "send DATA msg end");
    }

    // attachment
    if (att_path != NULL){
        if (gen_base64_file(att_path) == -1){
            perror("gen_base64_file failed file not exist");
            exit(EXIT_FAILURE);
        }
        else{
            sprintf(
                buf,
                "--%s\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Content-Transfer-Encoding: base64\r\n"
                "Content-Disposition: attachment; filename=\"%s\"\r\n\r\n",
                boundary, att_path);
            
            custom_send(s_fd, (void *)buf, strlen(buf), 0, "send DATA attachment header");

            char *att_base64 = file2str("attach_base64");
            custom_send(s_fd, (void *)att_base64, strlen(att_base64), 0, "send DATA attachment base64 file");
            free(att_base64);
        }

        sprintf(buf, "--%s\r\n", boundary);
        custom_send(s_fd, (void *)buf, strlen(buf), 0, "send DATA attachment end");
    }

    
    // TODO: Message ends with a single period
    custom_send(s_fd, (void *)end_msg, strlen(end_msg), 0, "send .");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv .");

    // TODO: Send QUIT command and print server response
    const char* QUIT = "QUIT\r\n";
    custom_send(s_fd, (void *)QUIT, strlen(QUIT), 0, "send QUIT");
    custom_recv(s_fd, (void *)buf, MAX_SIZE, 0, "recv QUIT");

    // Close the socket
    close(s_fd);
}

int main(int argc, char* argv[])
{
    int opt;
    char* s_arg = NULL;
    char* m_arg = NULL;
    char* a_arg = NULL;
    char* recipient = NULL;
    const char* optstring = ":s:m:a:";
    while ((opt = getopt(argc, argv, optstring)) != -1)
    {
        switch (opt)
        {
        case 's':
            s_arg = optarg;
            break;
        case 'm':
            m_arg = optarg;
            break;
        case 'a':
            a_arg = optarg;
            break;
        case ':':
            fprintf(stderr, "Option %c needs an argument.\n", optopt);
            exit(EXIT_FAILURE);
        case '?':
            fprintf(stderr, "Unknown option: %c.\n", optopt);
            exit(EXIT_FAILURE);
        default:
            fprintf(stderr, "Unknown error.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        fprintf(stderr, "Recipient not specified.\n");
        exit(EXIT_FAILURE);
    }
    else if (optind < argc - 1)
    {
        fprintf(stderr, "Too many arguments.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        recipient = argv[optind];
        send_mail(recipient, s_arg, m_arg, a_arg);
        exit(0);
    }
}
