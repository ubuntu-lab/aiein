/*
作用: 
1. 使用C语言模拟http协议
2. 向reqs中的网址发送请求
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 


#ifdef	__cplusplus
extern "C"
#endif


// 请求头
#define HTTP_VERSION    "HTTP/1.1"
#define USER_AGENT      "User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:10.0.2) Gecko/20100101 Firefox/10.0.2\r\n"
#define ENCODE_TYPE     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
#define CONNECTION_TYPE "Connection: close\r\n"


#define BUFFER_SIZE     4096


// 将网络地址转化成IP字符串
char *host_to_ip(const char *hostname) {

	// 获取主机地址
	struct hostent *host_entry = gethostbyname(hostname); //gethostbyname_r / getaddrinfo
	if (host_entry) {
		// 将网络地址转化成IP字符串 
		return inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list);
	}
	else {
		return NULL;
	}
}

// 创建套接字
int http_create_socket(char *ip) {
	// 套接字
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sin = { 0 };
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(80);  // 主机字节转化成网络
	sin.sin_family = AF_INET;

	if (-1 == connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
		return -1;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	return sockfd;

}


// 发送请求
char *http_send_request(int sockfd, const char *hostname, const char *resource) {

	char buffer[BUFFER_SIZE] = { 0 };

	int len = sprintf(buffer,
"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n",
	resource, HTTP_VERSION,
	hostname,
	CONNECTION_TYPE
);
	printf("request buffer:%s\n", buffer);

	// 发送数据
	send(sockfd, buffer, strlen(buffer), 0);

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	fd_set fdread;
	FD_ZERO(&fdread);
	FD_SET(sockfd, &fdread);

	char *result = (char *)malloc(sizeof(int));
	result[0] = '\0';

	while (1) {
		// 等待服务器响应
		int selection = select(sockfd + 1, &fdread, NULL, NULL, &tv);

		if (!selection || !(FD_ISSET(sockfd, &fdread))) {
			break;
		} else {
			len = recv(sockfd, buffer, BUFFER_SIZE, 0);
			if (len == 0) {
				break;
			}

			result = (char *)realloc(result, (strlen(result) + len + 1) * sizeof(char));
			strncat(result, buffer, len);
		}
	}

	return result;
}

// 发送请求
int http_client_commit(const char *hostname, const char *resource) {
	// 将host转化成ip
	char *ip = host_to_ip(hostname);
	// 获取主机ip
	int sockfd = http_create_socket(ip);
	// 发送请求
	char *content = http_send_request(sockfd, hostname, resource);
	if (content == NULL) {
		printf("have no data\n");
	}
	// 打印结果
	puts("============ http_client_commit:content ============");
	puts(content);

	// 关闭socket
	close(sockfd);
	// 释放http接收值
	free(content);
	return 0;
}

// http请求结构体
struct http_request {
	char *hostname;  // url
	char *resource;  // 资源
};

// 请求位置
struct http_request reqs[] = {
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=beijing&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=changsha&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=shenzhen&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=shanghai&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=tianjin&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=wuhan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=hefei&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=hangzhou&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=nanjing&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=jinan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=taiyuan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=wuxi&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=suzhou&language=zh-Hans&unit=c" },
};


int main(int argc, char *argv[]) {
	// 请求数量
	int count = sizeof(reqs) / sizeof(reqs[0]);
	int i = 0;
	// 发送请求
	for (i = 0; i < count; i++) {
	// 发送请求
/*
	hostname : "api.seniverse.com",
	resource : "/v3/weather/now.json?key=0pyd8z7jouficcil&location=beijing&language=zh-Hans&unit=c"
*/
		http_client_commit(reqs[i].hostname, reqs[i].resource);
	}

	return 0;
}