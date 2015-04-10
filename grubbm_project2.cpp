/* Network Programming Project 2: MALEA GRUBB */

#include <string.h>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <map>
#include <vector>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include <assert.h>
#include <cctype>

#define BUF_SIZE 400 
#define PORT 34567

using namespace std;

mutex peer_table_mutex;


/* Helper Function that returns a vector of strings split by a chosen deliminator */
vector<string> tokenize(string str, char delim) {
  vector<string> tokens;
  stringstream ss(str);
  string item;
  while (getline(ss, item, delim)) {
    tokens.push_back(item);
  }
  return tokens;
}
/* Helper function to remove all non alphanumeric characters from a string, then add '\0' to end */
string remove_junk(string str) {
  size_t i = 0;
  size_t len = str.length();
  while (i < len) {
    if (!isalnum(str[i])) {
        str.erase(i,1);
        len--;
    }
    else {
      i++;
    }
  }
  return str;
}
/* Helper function to retrieve a username from a string */
string get_username(string message, string which) {
  string username;
  vector<string> tokens;
  vector<string> tokens2;
  tokens = tokenize(message, ' ');

  if (which == "REGISTRATION") {
    return remove_junk(tokens[1]);
  }
  else if (which == "FROM") {
    tokens2 = tokenize(tokens[1], ':');
    return remove_junk(tokens2[1]);
  }
  else if (which == "TO") {
    tokens2 = tokenize(tokens[2], ':');
    return remove_junk(tokens2[1]);
  }
}

/* Helper function that determines whether a string matches a pattern */
bool is_match(string str, string pattern) {
  bool is_match = true;
  for (int i = 0; i < pattern.size(); i++) {
    if (str[i] == pattern[i]) {
      continue;
    }
    is_match = false;
    break;
  }
  return is_match;
}

/* Helper function that returns true if buffer is from register call, otherwise false */
bool is_registration(string message) {
  string pattern = "REGISTER";
  return is_match(message, pattern);
}

/* Helper Function that returns true if buffer is intended to make a call, otherwise false */
bool wants_to_make_call(string message) {
  vector<string> tmp;
  vector<string> tokens = tokenize(message, ' ');
  // message must start with "CALL"
  if (tokens[0] != "CALL") {
    return false;
  }
  // look for "FROM" 
  tmp = tokenize(tokens[1], ':');
  if (tmp[0] != "FROM") {
    return false;
  }
  // look for "TO" 
  tmp = tokenize(tokens[2], ':');
  if (tmp[0] != "TO") {
    return false;
  }
  // looks like it is a valid "CALL FROM #user TO #anotheruser"
  // note : this function DOES not mean both users are registered, this handled elsewhere
  return true;
}

/* Helper function that determines if a call was aknowledged from a recipient */
bool call_was_ack(string message) {
  vector<string> tokens = tokenize(message, ' ');
  if (tokens[0] != "ACK_CALL") {
    return false;
  }
  return true;
}

/* This thread serves the media */
void serve_media(struct sockaddr_in6 src, struct sockaddr_in6 dest, socklen_t srclen, 
                 socklen_t destlen, int proxy_sockfd) {
  struct sockaddr_in6 source;
  int ret, ret2;
  char buf[BUF_SIZE], cnt[60], clnt[60], svc[80], svce[80];
  
  // media port loop
  while (true) {
    char buf[BUF_SIZE];
    memset(&buf, 0, BUF_SIZE);
    memset(&cnt, 0, 60);
    memset(&svc, 0, 80);
    memset(&clnt, 0, 60);
    memset(&svce, 0, 80);

    // receive a packet
    socklen_t size_source = sizeof(source);
    ret = recvfrom(proxy_sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&source, &size_source);
    if (ret == -1) {
      perror("recvfrom in media serving");
      exit(1);
    }
   
    // use getnameinfo() to see who sent it, matching it against one of the clients 
    ret = getnameinfo((sockaddr*)&source, srclen, cnt, sizeof(cnt), svc, sizeof(svc), 0 | NI_NUMERICHOST);
    if (ret != 0) {
      cerr << "getnameinfo(): " << gai_strerror(ret) << endl;
      exit(1);
    }
    ret = getnameinfo((sockaddr*)&src, srclen, clnt, sizeof(clnt), svce, sizeof(svce), 0 | NI_NUMERICHOST);
    if (ret != 0) {
      cerr << "getnameinfo(): " << gai_strerror(ret) << endl;
      exit(1);
    }
    string current = string(cnt, sizeof(cnt));
    string pattern = string(clnt, sizeof(clnt));
    // if a match, send to other client
    if (current == pattern) {
      ret = sendto(proxy_sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&src, srclen);
    }
    // otherwise send it to the client it was not a match for
    else {
      ret = sendto(proxy_sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&dest, destlen);
    }
    if (ret == -1) {
      perror("sendto in media serving");
      exit(1);
    }
  }
  close(proxy_sockfd);
}

/* MAIN */
int main(int argc, char *argv[]) {
  map<string,string> user_table;
  int sockfd, proxy_sockfd, ret;
  struct sockaddr_in6 src, bindaddr, proxyaddr;
  socklen_t srclen;
  ssize_t size;
  int next_port = 5000;

  #ifdef TEST_MODE
    string test = "REGISTER yodaballer1na";
    string test2 = "CALL FROM:yodaballer1na TO:maleangrubb35";
    string test3 = "ACK_CALL FROM:yodaballerina TO:maleangrubb35";
    string test4 = "CALL_FAILED FROM:yodaballerina TO:maleangrubb35";
    vector<string> test_vector;
    test_vector.push_back("CALL");
    test_vector.push_back("FROM:yodaballer1na");
    test_vector.push_back("TO:maleangrubb35");

    assert(get_username(test, "REGISTRATION") == "yodaballer1na");
    assert(get_username(test2, "TO") == "maleangrubb35");
    assert(get_username(test2, "FROM") == "yodaballer1na");
    assert(is_registration(test));
    assert(!(is_registration(test2)));
    assert(tokenize(test2, ' ') == test_vector);
    assert(tokenize(test2, ':') != test_vector);
    assert(wants_to_make_call(test2));
    assert(!(wants_to_make_call(test)));
    assert(call_was_ack(test3));
    assert(!(call_was_ack(test4)));
    assert(!(call_was_ack(test2)));
    cout << "SUCCESS ON ALL TESTS!" << endl;
    return EXIT_SUCCESS;
  #endif

  // set up socket options
  bindaddr.sin6_family = AF_INET6;
  bindaddr.sin6_port = htons(PORT);
  bindaddr.sin6_addr = in6addr_any;

  // create IPv6 UDP socket
  sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  // bind the socket to the proper port
  ret = bind(sockfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr));
  if (ret != 0) {
    perror("bind");
    return EXIT_FAILURE;
  }

  // receive loop on port 34567
  while (true) {
    char buf[BUF_SIZE], cnt[60], svc[80];
    memset(&buf, 0, BUF_SIZE);
    memset(&cnt, 0, 60);
    memset(&svc, 0, 80);
    srclen = sizeof(src);

    // receive message
    size = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&src, &srclen);
    if (size == -1) {
      perror("recvfrom");
      return EXIT_FAILURE;
    }

    // convert the buffer to a string
    string b = string(buf, 60);

    // get address for user table
    ret = getnameinfo((sockaddr*)&src, srclen, cnt, sizeof(cnt), svc, sizeof(svc), 0 | NI_NUMERICHOST);
    if (ret != 0) {
      cerr << "getnameinfo(): " << gai_strerror(ret) << endl;
      return EXIT_FAILURE;
    }

    // check if buffer is a registration
    if (is_registration(b)) {

      // get username for user table
      string username = get_username(b, "REGISTRATION");

      // add username and address to the user table
      string client_address = string(cnt, strlen(cnt));
      auto pair = user_table.emplace(username, client_address);
      // acknowledge registration to client
      string ack = "ACK_REGISTER " + username;
      ret = sendto(sockfd, ack.c_str(), ack.size(), 0, (struct sockaddr *)&src, srclen);
      if (ret == -1) {
        perror("sendto");
        return EXIT_FAILURE;
      }
      continue;
    }

    // check if the buffer is a call notification
    if (wants_to_make_call(b)) {
      // parse destination and recipient usernames
      string from_user = get_username(b, "FROM");
      string to_user = get_username(b, "TO");
     
      // look up the recipient in the user table
      map<string,string>::iterator itr = user_table.find(to_user);

      // if recipient is not in the user table, send CALL_FAILED message to calling user
      if (itr == user_table.end()) {
        string fail = "CALL_FAILED unknown peer";
        ret = sendto(sockfd, fail.c_str(), fail.size(), 0, (struct sockaddr *)&src, srclen);
        continue;
      }

      // recipient found
      else {

        // grab destination address string
        string a = itr->second;

        // set up destination address, convert string address to sockaddr_in6 address
        struct sockaddr_in6 dest;
        socklen_t destlen;
        dest.sin6_family = AF_INET6;
        dest.sin6_port = htons(PORT);
        ret = inet_pton(AF_INET6, a.c_str(), &dest.sin6_addr);
        if (ret == 0) {
          cerr << "error converting address to sin6_addr" << endl;
          return EXIT_FAILURE;
        }

        // send call notification to destination
        string note = "CALL FROM:" + from_user + " TO:" + to_user;
        destlen = sizeof(dest);
        ret = sendto(sockfd, note.c_str(), note.size(), 0, (struct sockaddr *)&dest, destlen);
        if (ret == -1) {
          perror("sendto");
          cout<< "HERE" << endl;
          return EXIT_FAILURE;
        }

        // now we need the response from the recipient
        char res[BUF_SIZE];
        memset(&res, 0, BUF_SIZE);
        ret = recvfrom(sockfd, res, BUF_SIZE, 0, (struct sockaddr *)&dest, &destlen);
        if (ret == -1) {
          perror("recvfrom");
          return EXIT_FAILURE;
        }

        // send response to call originator
        ret = sendto(sockfd, res, BUF_SIZE, 0, (struct sockaddr *)&src, srclen);
        if (ret == -1) {
          perror("sendto");
          return EXIT_FAILURE;
        }

        // if the response was that the call failed, continue
        if (!call_was_ack(note)) {
          continue;
        }
        
        // choose port
        int proxy_port = next_port;
        next_port += 1; 
        
        // set up socket options
        proxyaddr.sin6_family = AF_INET6;
        proxyaddr.sin6_port = htons(proxy_port);
        proxyaddr.sin6_addr = in6addr_any;

        // create IPv6 UDP socket
        proxy_sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (proxy_sockfd == -1) {
          perror("socket");
          return EXIT_FAILURE;
        }

        // bind the socket to the proper port
        ret = bind(proxy_sockfd, (struct sockaddr *)&proxyaddr, sizeof(proxyaddr));
        if (ret != 0) {
          perror("bind");
          return EXIT_FAILURE;
        }

        // send media port announcing message to each peer
        string port_str = to_string(proxy_port);
        note = "MEDIA_PORT FROM:" + from_user + " TO:" + to_user + " " + port_str; 
        ret = sendto(proxy_sockfd, note.c_str(), note.size(), 0, (struct sockaddr *)&dest, destlen);
        size = sendto(proxy_sockfd, note.c_str(), note.size(), 0, (struct sockaddr *)&src, srclen);
        if (ret == -1 || size == -1) {
          perror("sendto");
          return EXIT_FAILURE;
        } 
       
        // start a new thread to handle media path 
        thread media_path(serve_media, src, dest, srclen, destlen, proxy_sockfd);
      }
    }
    // we have received an improperly formatted message
    else {
      cerr << "Invalid message received! We will continue" << endl;
      continue;
    }
  }
  close(sockfd);
  return EXIT_SUCCESS;
}
