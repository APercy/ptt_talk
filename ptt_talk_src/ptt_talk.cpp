/** @ptt talk.cpp
    @brief Record input into an ogg file; Playback recorded data.
    @author APercy - adapted code from Phil Burk  http://www.softsynth.com
*/
/*
 * $Id$
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */


#ifdef _WIN32
    #include <winsock2.h>
	#include <windows.h>
    #include "mingw_tread/mingw.thread.h"
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
	#include <sys/mman.h>
	#include <fcntl.h> /* For O_* constants */

    #include<X11/X.h>
    #include<X11/Xlib.h>
    #include<X11/Xutil.h>
    #include <thread>
#endif // _WIN32

#include <unistd.h> //for sleep, read, write

#include <strings.h>
#include <string.h>

#include <sys/stat.h>
#include <fstream>

#include "portaudio.h"
#include "ptt_talk.h"
#include "audio_rec_play.h"
#include "sine.h"

char *hostname;
char *nick;
bool capture_key = false;

/*******************************************************************/
int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;

    /* check command line arguments */
    if (argc != 3) {
       #ifdef _WIN32
       fprintf(stderr,"usage: %s <server IP addresss> <nick>\n", argv[0]);
       #else
       fprintf(stderr,"usage: %s <server host name> <nick>\n", argv[0]);
       #endif // _WIN32
       exit(0);
    }
    hostname = argv[1];
    nick = argv[2];

    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;
    bool                should_stop = false;
    bool                is_recording = false;

    printf("ptt_talk.cpp\n"); fflush(stdout);

    float last_time = 0.0;
    float sleep_time = 0.3;

    #ifdef _WIN32
    SHORT keyState = GetKeyState(VK_CAPITAL/*(caps lock)*/);
    bool isToggled = keyState & 1;
    bool isDown = keyState & 0x8000;
    initialize(&inputParameters, stream, err, &data, &numSamples, &numBytes, &totalFrames, &should_stop);
    capture_key = true;
    while (1) {
        Sleep(sleep_time);

        last_time += sleep_time;
        if(last_time > 0.5) last_time = 0.5;

        keyState = GetAsyncKeyState(VK_MENU);
		if(keyState != 0 && keyState != -32768) printf("%d\n", keyState);

        /* Record some audio. -------------------------------------------- */
        if( keyState == -32768 && capture_key) { //--- ALT key
            //printf ("Alt pressed!\n");
            if( is_recording == false ) { //isn't recording
                if(last_time >= 0.5) {
                    last_time = 0.0;
                    play_sine();
                    printf(">>> starting\n");
                    is_recording = true; //so start the record
                    should_stop = false;
                    initialize(&inputParameters, stream, err, &data, &numSamples, &numBytes, &totalFrames, &should_stop);
                    //recordAudio(inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                    std::thread thread(recordAudio, inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                    thread.detach();
                }
            } else {
                if (should_stop == false && is_recording == true) {
                    if(last_time >= 0.5) {
                        last_time = 0.0;
                        printf(">>> stopping\n");
                        should_stop = true;
                    }
                }
            }
        }

    }

    #else

    //linux
    /* ------------------- set key event ------------------- */
    Display* display = XOpenDisplay(NULL);
    if(display == NULL) {
        fprintf(stderr,"Cannot open display.\n");
        return done(&data, err);
    }
    int screen = DefaultScreen(display);
    Window root = DefaultRootWindow(display);
    /* create window */
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 200, 200, 1,
                           BlackPixel(display, screen), WhitePixel(display, screen));

    Window curFocus = window;
    char buf[17];
    KeySym ks;
    XComposeStatus comp;
    int len;
    int revert;

    unsigned int keycode = XKeysymToKeycode(display, XK_Tab);
    XGetInputFocus (display, &curFocus, &revert);
    XSelectInput(display, window, KeyPressMask|KeyReleaseMask|FocusChangeMask);
    /* map (show) the window */
    XMapWindow(display, window);

    XEvent ev;
    /* ------------------- end key event ------------------- */

    initialize(&inputParameters, stream, err, &data, &numSamples, &numBytes, &totalFrames, &should_stop);
    while (1)
    {
        sleep(sleep_time);

        last_time += sleep_time;
        if(last_time > 0.5) last_time = 0.5;
        XNextEvent(display, &ev);
        switch (ev.type) {
            case FocusOut:
                /*printf ("Process changed!\n");
                printf ("Old Process is %d\n", (int)curFocus);*/
                if (curFocus != root) {
                    XSelectInput(display, curFocus, 0);
                }
                XGetInputFocus (display, &curFocus, &revert);
                //printf ("New Process is %d\n", (int)curFocus);
                if (curFocus == PointerRoot) {
                    curFocus = root;
                }
                XSelectInput(display, curFocus, KeyPressMask|KeyReleaseMask|FocusChangeMask);
                if(1) {
                    char *window_name = NULL;
                    if(XFetchName(display, curFocus, &window_name) > 0) {
                        std::string str_name = (std::string) window_name;
                        if(str_name.find("Minetest")!=std::string::npos) {
                            //printf ("É o Minetest!!!! %s\n", window_name);
                            capture_key = true;
                        } else {
                            capture_key = false;
                        }
                        XFree(window_name);
                    }
                }
                break;
            case KeyPress:
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                /*printf ("Got key!\n");
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                if (len > 0 && isprint(buf[0]))
                {
                    buf[len]=0;
                    printf("String is: %s\n", buf);
                }
                else
                {
                    printf ("Key is: %d\n", (int)ks);
                }*/

                /* Record some audio. -------------------------------------------- */
                if( ks == 65513 && capture_key) { //--- ALT key
                    printf ("Alt pressed!\n");
                    if( is_recording == false ) { //isn't recording
                        if(last_time >= 0.5) {
                            last_time = 0.0;
                            play_sine();
                            printf(">>> starting\n");
                            is_recording = true; //so start the record
                            should_stop = false;
                            initialize(&inputParameters, stream, err, &data, &numSamples, &numBytes, &totalFrames, &should_stop);
                            //recordAudio(inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                            std::thread thread(recordAudio, inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                            thread.detach();
                        }
                    } else {
                        if (should_stop == false && is_recording == true) {
                            if(last_time >= 0.5) {
                                last_time = 0.0;
                                printf(">>> stopping\n");
                                should_stop = true;
                            }
                        }
                    }
                }

                /* Playback recorded data.  -------------------------------------------- */
                //playbackAudio( outputParameters, stream, err, data);
                break;

        } //end switch

    } //end while
    #endif
}

#ifdef _WIN32
int sendMessage(char* filepath) {
    WSADATA              wsaData;
    SOCKET               SendingSocket;
    // Server/receiver address
    SOCKADDR_IN          ServerAddr, ThisSenderInfo;
    // Server/receiver port to connect to
    unsigned int         Port = 41000;
    int  RetCode;

    // Be careful with the array bound, provide some checking mechanism...
    char sendbuf[256];
    int BytesSent, nlen;

    // Initialize Winsock version 2.2
    WSAStartup(MAKEWORD(2,2), &wsaData);
    printf("Client: Winsock DLL status is %s.\n", wsaData.szSystemStatus);

    // Create a new socket to make a client connection.
    // AF_INET = 2, The Internet Protocol version 4 (IPv4) address family, TCP protocol
    SendingSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(SendingSocket == INVALID_SOCKET)
    {
        printf("Client: socket() failed! Error code: %ld\n", WSAGetLastError());
        // Do the clean up
        WSACleanup();
        // Exit with error
        return -1;
    }
    else
      printf("Client: socket() is OK!\n");

    // Set up a SOCKADDR_IN structure that will be used to connect
    // to a listening server on port 41000
    // IPv4
    ServerAddr.sin_family = AF_INET;
    // Port no.
    ServerAddr.sin_port = htons(Port);
    // The IP address
    ServerAddr.sin_addr.s_addr = inet_addr(hostname);

    // Make a connection to the server with socket SendingSocket.
    RetCode = connect(SendingSocket, (SOCKADDR *) &ServerAddr, sizeof(ServerAddr));
    if(RetCode != 0)
    {
        printf("Client: connect() failed! Error code: %ld\n", WSAGetLastError());
        // Close the socket
        closesocket(SendingSocket);
        // Do the clean up
        WSACleanup();
        // Exit with error
        return -1;
    }
    else
    {
        printf("Client: connect() is OK, got connected...\n");
        printf("Client: Ready for sending and/or receiving data...\n");
    }

    // At this point you can start sending or receiving data on
    // the socket SendingSocket.
    // Some info on the receiver side...
    getsockname(SendingSocket, (SOCKADDR *)&ServerAddr, (int *)sizeof(ServerAddr));
    printf("Client: Receiver IP(s) used: %s\n", inet_ntoa(ServerAddr.sin_addr));
    printf("Client: Receiver port used: %d\n", htons(ServerAddr.sin_port));

     // Sends some data to server/receiver...
    memset((char*) sendbuf, 0, sizeof(sendbuf));
    strncpy(sendbuf, nick, sizeof(sendbuf));
    strncat(sendbuf, (const char*)"\n", sizeof(sendbuf - 1));
    BytesSent = send(SendingSocket, sendbuf, strlen(sendbuf), 0);

    if(BytesSent == SOCKET_ERROR)
        printf("Client: send() error %ld.\n", WSAGetLastError());
    else
    {
        printf("Client: send() is OK - bytes sent: %ld\n", BytesSent);
        // Some info on this sender side...
        // Allocate the required resources
        memset(&ThisSenderInfo, 0, sizeof(ThisSenderInfo));
        nlen = sizeof(ThisSenderInfo);

        getsockname(SendingSocket, (SOCKADDR *)&ThisSenderInfo, &nlen);
        printf("Client: Sender IP(s) used: %s\n", inet_ntoa(ThisSenderInfo.sin_addr));
        printf("Client: Sender port used: %d\n", htons(ThisSenderInfo.sin_port));
        printf("Client: Those bytes represent: \"%s\"\n", sendbuf);

        memset((char*) sendbuf, 0, sizeof(sendbuf));
        //Receive a reply from the server
        if((nlen = recv(SendingSocket, sendbuf, sizeof(sendbuf)-1 , 0)) == SOCKET_ERROR)
        {
	        printf("ERROR reading from socket\n");
            return -1;
        }
        printf("Now sending the file\n");
        //printf("Socket return %s\n",buffer);
        std::string str_buffer = (std::string) sendbuf;
        if(str_buffer == (std::string) "file\n") {
            //printf("Lets's send file\n");
            int64_t rc = SendFile(SendingSocket, "recorded.ogg");
            if (rc < 0) {
                printf("Failed to send file: %ld\n", rc);
            }
        }
    }

    if( shutdown(SendingSocket, SD_SEND) != 0)
        printf("Client: Well, there is something wrong with the shutdown(). The error code: %ld\n", WSAGetLastError());
    else
        printf("Client: shutdown() looks OK...\n");

    // When you are finished sending and receiving data on socket SendingSocket,
    // close the socket using the closesocket API.
    if(closesocket(SendingSocket) != 0)
        printf("Client: Cannot close \"SendingSocket\" socket. Error code: %ld\n", WSAGetLastError());
    else
        printf("Client: Closing \"SendingSocket\" socket...\n");

    // When your application is finished handling the connection, call WSACleanup.
    if(WSACleanup() != 0)
        printf("Client: WSACleanup() failed!...\n");
    else
        printf("Client: WSACleanup() is OK...\n");

    return 0;
}
#else
int sendMessage(char* file_path) {
    int sockfd, portno, n = 0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    portno = 41000;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        printf("ERROR opening socket\n");
        exit(0);
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    //printf("socket openned");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memmove((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);

    //printf("ok here\n");
    int con_result = connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (con_result < 0) {
        printf("ERROR connecting %d\n", con_result);
        exit(1);
    }
    //printf("ok here too\n");


    memset((char*) buffer, 0, sizeof(buffer));
    strncpy(buffer, nick, sizeof(buffer));
    strncat(buffer, (const char*)"\n", sizeof(buffer - 1));
    //n = send(sockfd , buffer , strlen(nick) , 0 );
    n = write(sockfd,buffer,strlen(buffer));
    //printf ("return write %d\n", (int)n);
    if (n < 0)
         printf("ERROR writing to socket");
    memset((char*) buffer, 0, sizeof(buffer));
    n = read(sockfd,buffer,255);
    if (n < 0)
         printf("ERROR reading from socket");
    //printf("Socket return %s\n",buffer);
    std::string str_buffer = (std::string) buffer;
    if(str_buffer == (std::string) "file\n") {
        //printf("Lets's send file\n");
        int64_t rc = SendFile(sockfd, "recorded.ogg");
        if (rc < 0) {
            printf("Failed to send file: %ld\n", rc);
        }
    }
    close(sockfd);
    return 0;
}
#endif

///
/// Sends data in buffer until bufferSize value is met
///
int SendBuffer(int socket, const char* buffer, int bufferSize, int chunkSize) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(socket, &buffer[i], std::min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

//
// Sends a file
// returns size of file if success
// returns -1 if file couldn't be opened for input
// returns -2 if couldn't send file length properly
// returns -3 if file couldn't be sent properly
//
int64_t SendFile(int socket, const std::string& fileName, int chunkSize) {

    const int64_t fileSize = GetFileSize(fileName);
    printf("File size: %ld\n", fileSize);
    if (fileSize < 0) { return -1; }

    std::ifstream file(fileName, std::ifstream::binary);
    if (file.fail()) { return -1; }
    //printf("Stream created\n");

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = std::min(i, (int64_t)chunkSize);
        if (!file.read(buffer, ssize)) {
            errored = true;
            break;
        }
        const int l = SendBuffer(socket, buffer, (int)ssize);
        //printf("buffer__%s\n", buffer);
        if (l < 0) {
            errored = true;
            break;
        }
        i -= l;
    }
    close(socket);
    delete[] buffer;

    file.close();
    printf("Audio sent.");

    return errored ? -3 : fileSize;
}

