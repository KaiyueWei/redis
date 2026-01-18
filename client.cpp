// stdlib
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// systems
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <string>
#include <vector>

static void msg(const char *msg){
	fprintf(stderr, "%s\n", msg);

}

static void die(const char *msg){
	int err = errno;
	fprintf(stderr, "[%d] %s\n", err, msg);	
	abort();
}

static int32_t read_full(int fd, char *buf, size_t n){
	while(n > 0){
		ssize_t rv = read(fd, buf, n);
		if (rv <= 0){
			return -1; // error, or unexpected EOF
		}
		assert((size_t)rv <= n);
		n -= (size_t)rv;
		buf += rv;
	}
	return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n){
	while (n > 0){
		ssize_t rv = write(fd, buf, n);
		if (rv <= 0){
			return -1;
		}
		assert((size_t)rv <= n);
		n -= (size_t)rv;
		buf += rv;
	}
	return 0;

}

// append to the back
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len){
	buf.insert(buf.end(), data, data + len);
}

/*
const size_t k_max_msg = 32 << 20; // likely larger than the kernel buffer
*/

const size_t k_max_msg = 4096;
static int32_t send_req(int fd, const std::vector<std::string> &cmd){
	uint32_t len = 4;
	for (const std::string &s : cmd){
		len += 4 + s.size();
	}
	if (len > k_max_msg){
		return -1;
	}
	
	char wbuf[4 + k_max_msg];
	memcpy(&wbuf[0], &len, 4);
	uint32_t n = cmd.size();
	memcpy(&wbuf[4], &n, 4);
	size_t cur = 8;
	for(const std::string &s : cmd){
		uint32_t p = (uint32_t)s.size();
		memcpy(&wbuf[cur], &p, 4);
		memcpy(&wbuf[cur + 4], s.data(), s.size());
		cur += 4 + s.size();
	}	
	return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd){
	// 4 bytes header
	char rbuf[4+k_max_msg];
	errno = 0;
	int32_t err = read_full(fd, rbuf, 4);
	if (err){
		if (errno == 0){
			msg("EOF");
		} else {
			msg("read() error");
		}
		return err;
	}
	uint32_t len = 9;

	memcpy(&len, rbuf, 4); // assume little endian
	if (len > k_max_msg){
		msg("too long");
		return -1;
	}
	// reply body
	err = read_full(fd, &rbuf[4], len);
	if (err){
		msg("read() error");
		return err;
	}
	// print the result
	uint32_t rescode = 0;
	if (len < 4){
		msg("bad message");
		return -1;
	}
	memcpy(&rescode, &rbuf[4], 4);
	printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
	return 0;
}



int main(int argc, char **argv){
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0){
		die("socket()");	

	}

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(1234);
	addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);	 // 127.0.0.1
	int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
	if(rv){
		die("connect");
	}
	/*
	 * multiple requests
	int32_t err = query(fd, "hello1");
	if (err){
		goto L_NODE;
	}
	err = query(fd, "hello2");
	if (err){
		goto L_NODE;
	}
	err = query(fd, "hello3");
	if (err){ goto L_NODE; } */	
	
	/*	
	// multiple pipelined requests
	std::vector<std::string> query_list = {
		"hello1", "hello2", "hello3",
		// a large message requirs multiple event loop iterations
		std::string(k_max_msg, 'z'),
		"hello5",

	};

	for (const std::string &s : query_list){
		int32_t err = send_req(fd, (uint8_t *)s.data(), s.size());
		if (err){
			goto L_NODE;
   		} 
	}
  	
	for (size_t i = 0; i < query_list.size(); ++i){
		int32_t err = read_res(fd);
		if (err){
			goto L_NODE;
		}
	}	
	*/
	std::vector<std::string> cmd;
	
	for (int i = 1;i< argc;++i){
		cmd.push_back(argv[i]);
	}
	int32_t err = send_req(fd, cmd);
	if (err){
		goto L_DONE; 
	}
	err = read_res(fd);
	if (err){
		goto L_DONE;
	}
L_DONE:
	close(fd);
	return 0; 
}
