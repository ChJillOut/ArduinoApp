/*
 This code primarily comes from
 http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
 and
 http://www.binarii.com/files/papers/c_sockets.txt
 */

//./server /dev/cu.usbmodem14101 3001
// gcc -o server server.c
//kill -9 $(lsof -t -i:3001)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>

char buffer[1];
char temperature[100];
int count = 0;
void configure(int fd);
void* read_temp(void* fd);
void check_connection();
void reconnect();
void* close_server(void* p);
void* handle_request(void* thread_data);

pthread_mutex_t lock;
float temp_number;
int serial_fd;
char filename[50];
int disconnected = 0;
int quit = 0;
int stand_by = 0;
int is_celsius = 1;

//used to compute avergae, low and high
float temps[100];
int temp_count = 0;
float sum_temp = 0;
float max_temp = 0;
char max_temp_str[20];
float min_temp = 200;
char min_temp_str[20];
float average_temp = 0;
char average_temp_str[20];


/*
 This code configures the file descriptor for use as a serial port.
 */
void configure(int fd) {
    struct termios pts;
    tcgetattr(fd, &pts);
    cfsetospeed(&pts, 9600);
    cfsetispeed(&pts, 9600);
    
    /*
     // You may need to un-comment these lines, depending on your platform.
     pts.c_cflag &= ~PARENB;
     pts.c_cflag &= ~CSTOPB;
     pts.c_cflag &= ~CSIZE;
     pts.c_cflag |= CS8;
     pts.c_cflag &= ~CRTSCTS;
     pts.c_cflag |= CLOCAL | CREAD;
     pts.c_iflag |= IGNPAR | IGNCR;
     pts.c_iflag &= ~(IXON | IXOFF | IXANY);
     pts.c_lflag |= ICANON;
     pts.c_oflag &= ~OPOST;
     */
    
    tcsetattr(fd, TCSANOW, &pts);
    
}

/*
 Read temperature from Arduino
 */
void* read_temp(void* fd) {
    
    /*
     Write the rest of the program below, using the read and write system calls.
     */
    int has_temp = 0;
    while(1) {
        if(quit == 1){
            return 0;
        }
        
        int bytes_read = read(*(int*)fd, buffer, 1);
        
        //read the input byte by byte
        if (bytes_read > 0) {
            //check if the current input is a new line character
            if (buffer[0] == '\n') {
                //check for valid temperature reading
                //has_temp will equal to zero if the first character is '\n'
                if(has_temp == 1){
                    pthread_mutex_lock(&lock);
                    temperature[count] = '\0';
                    count++;
                    printf("%s\n", temperature);
                    
                    const char* delimiter = " ";
                    char* token = strtok(temperature, delimiter);
                    int i;
                    //get the temperature reading from the input string
                    for (i = 0; i < 3; i++) {
                        token = strtok(NULL, delimiter);
                    }
                    
                    //store the temperature as a float
                    if(token != NULL){
                        temp_number = atof(token);
                    }
                    
                    //reset
                    count = 0;
                    has_temp = 0;
                    pthread_mutex_unlock(&lock);
                }
            }
            else {//keep storing until hit '\n'
                temperature[count] = buffer[0];
                count++;
                has_temp = 1;
            }
        }
    }
    return 0;
}


int start_server(int PORT_NUMBER)
{
    // structs to represent the server and client
    struct sockaddr_in server_addr,client_addr;
    
    int sock; // socket descriptor
    
    // 1. socket: creates a socket descriptor that you later use to make other system calls
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
    int temp;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
        perror("Setsockopt");
        exit(1);
    }
    
    // configure the server
    server_addr.sin_port = htons(PORT_NUMBER); // specify port number
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero),8);
    
    // 2. bind: use the socket and associate it with the port number
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Unable to bind");
        exit(1);
    }
    
    // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
    if (listen(sock, 1) == -1) {
        perror("Listen");
        exit(1);
    }
    
    // once you get here, the server is set up and about to start listening
    printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
    fflush(stdout);
    
    
    while (1) {
        if(quit == 1){
            break;
        }
        
        // 4. accept: wait here until we get a connection on that port
        int sin_size = sizeof(struct sockaddr_in);
        int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
        
        printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
        
        //create new thread to handle multiple request
        pthread_t t5;
        int *newfd = malloc(sizeof(int));
        *newfd = fd;
        pthread_create(&t5, NULL,&handle_request, newfd);
        pthread_detach(t5);
        
    }
    // 8. close: close the socket
    close(sock);
    printf("Server shutting down\n");
    
    return 0;
}

void* handle_request(void* thread_data){
    
    int fd = *(int*) thread_data;
    free(thread_data);
    if (fd != -1) {
        // buffer to read data into
        char request[1024];
        // 5. recv: read incoming message (request) into buffer
        int bytes_received = recv(fd,request,1024,0);
        
        // null-terminate the string
        request[bytes_received] = '\0';
        // print it to standard out
        printf("This is the incoming request:\n%s\n", request);
        
        //parse HTTP request command
        char command[4];
        int i;
        for(i = 0; i < 3; i++){
            command[i] = request[i];
        }
        command[3] = '\0';
        
        //parse HTTP request content
        char content[10];
        i = 4;
        for (int j = 0; i < 9; i++, j++) {
            content[j] = request[i];
        }
        content[6] = '\0';
        
        if (strcmp(command, "GET") == 0) {
            
            //check Arduino connection
            check_connection();
            if(disconnected == 1){
                reconnect();
            }
            
            char control = '0';
            if(strcmp(content, "/temp") != 0){
                
                char reply[10000];
                //read the html file into a string
                char buffer[100];
                char filename[] = "test.html";
                FILE * f = fopen (filename, "r");
                char html[5000];
                fgets(buffer, 100, f);
                buffer[strlen(buffer)-1] = '\0';
                strcpy(html, buffer);
                while(fgets(buffer, 100, f) != NULL){
                    //replace '\n' with '\0'
                    if(buffer[strlen(buffer)-1] == '\n'){
                        buffer[strlen(buffer)-1] = '\0';
                    }
                    if(buffer[0] != '\0'){
                        strcat(html, buffer);
                    }
                }
                fclose(f);
                
                strcpy(reply, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n");
                strcat(reply, html);
                //send the request to browser
                send(fd, reply, strlen(reply), 0);
                
                //contents and corresponding control signals
                if (strcmp(content, "/blue") == 0) {
                    control = 'b';
                }else if (strcmp(content, "/toCe") == 0) {
                    control = 'c';
                }else if (strcmp(content, "/toFa") == 0) {
                    control = 'f';
                }else if (strcmp(content, "/sdby") == 0) {
                    control = 's';
                    pthread_mutex_lock(&lock);
                    stand_by = 1;
                    pthread_mutex_unlock(&lock);
                }else if (strcmp(content, "/resu") == 0) {
                    control = 'r';
                    pthread_mutex_lock(&lock);
                    stand_by = 0;
                    pthread_mutex_unlock(&lock);
                }
                
                //write the control signal to Arduino if applicable
                if(control != '0'){
                    pthread_mutex_lock(&lock);
                    int n = write(serial_fd, &control, 1);
                    pthread_mutex_unlock(&lock);
                }
                
            }
            else{
                
                char json_response[500] = "";
                char temp_str[20];
                
                pthread_mutex_lock(&lock);
                if(stand_by){
                    //do not copy the current temp value into temp_str
                    //if at standy by mode
                    strcpy(temp_str, "standing by");
                }else{
                    sprintf(temp_str, "%f", temp_number);
                }
                
                //copy the statistics into corresponding str
                sprintf(min_temp_str, "%f", min_temp);
                sprintf(max_temp_str, "%f", max_temp);
                sprintf(average_temp_str, "%f", average_temp);
                
                //if Arduino is connected
                //count the total temp input
                //and compute the sum
                if(disconnected == 0){
                    temp_count++;
                    sum_temp += temp_number;
                }
                pthread_mutex_unlock(&lock);
                
                //construct the json response string
                strcpy(json_response, "{\"currTemp\":\"");
                strcat(json_response, temp_str);
                strcat(json_response, "\",");
                strcat(json_response, "\"averageTemp\":\"");
                strcat(json_response, average_temp_str);
                strcat(json_response, "\",");
                strcat(json_response, "\"minTemp\":\"");
                strcat(json_response, min_temp_str);
                strcat(json_response, "\",");
                strcat(json_response, "\"maxTemp\":\"");
                strcat(json_response, max_temp_str);
                strcat(json_response, "\"}");
                
                //if the Arduino is connected
                //send the json response to the browser
                if(disconnected == 0){
                    char reply1[150];
                    strcpy(reply1, "HTTP/1.1 200 OK\nContent-Type: application/json\n\n");
                    strcat(reply1, json_response);
                    send(fd, reply1, strlen(reply1), 0);
                }
            }
        }
    }
    // 7. close: close the connection
    close(fd);
    printf("Server closed connection\n");
    return 0;
}

void* compute_temp_stats(void* max_seconds) {
    
    while (1) {
        if(quit == 1){
            return 0;
        }
        float seconds = 0;
        clock_t start = clock();
        //waiting time
        while (seconds < *(float*)max_seconds) {
            clock_t end = clock();
            seconds = (float)(end - start) / CLOCKS_PER_SEC;
        }
        
        pthread_mutex_lock(&lock);
        
        //compute max temp
        if (temp_number > max_temp) {
            max_temp = temp_number;
        }
        
        //compute min temp
        if (temp_number < min_temp) {
            min_temp = temp_number;
        }
        
        //compute avg temp
        if (temp_count != 0) {
            average_temp = sum_temp / temp_count;
        }
        pthread_mutex_unlock(&lock);
        
    }
}

void reconnect() {
    pthread_mutex_lock(&lock);
    //try to open corresponding file
    serial_fd = open(filename, O_RDWR | O_NOCTTY | O_NDELAY);
    //Arduino is connected if fd > 0
    if (serial_fd >= 0) {
        disconnected = 0;
    }
    pthread_mutex_unlock(&lock);
}

void check_connection(){
    pthread_mutex_lock(&lock);
    //try to open corresponding file
    serial_fd = open(filename, O_RDWR | O_NOCTTY | O_NDELAY);
    //Arduino is disconnected if fd < 0
    if(serial_fd < 0){
        //set the flag to 1
        disconnected = 1;
        printf("%s\n", "Arduino is disconnected!");
    }
    pthread_mutex_unlock(&lock);
}

void* close_server(void* p){
    while(1){
        char line[100] = "";
        fgets(line, sizeof(line), stdin);
        //if the input equals to "q"
        if(strcmp(line, "q\n") == 0){
            pthread_mutex_lock(&lock);
            //set the flag to 1
            quit = 1;
            pthread_mutex_unlock(&lock);
            break;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // check the number of arguments
    if (argc != 3) {
        printf("Please specify the name of the serial port (USB) device file!\n");
        printf("\nUsage: %s [port_number]\n", argv[0]);
        exit(-1);
    }
    
    int port_number = atoi(argv[2]);
    if (port_number <= 1024) {
        printf("\nPlease specify a port number greater than 1024\n");
        exit(-1);
    }
    
    // get the name from the command line
    strcpy(filename, argv[1]);
    
    // try to open the file for reading and writing
    // you may need to change the flags depending on your platform
    serial_fd = open(filename, O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (serial_fd < 0) {
        perror("Could not open file\n");
        exit(1);
    }
    else {
        printf("Successfully opened %s for reading and writing\n", filename);
    }
    
    configure(serial_fd);
    pthread_t t1, t2, t3;
    pthread_mutex_init(&lock, NULL);
    
    //set the waiting time
    float max_seconds = 10;
    pthread_create(&t1, NULL, &read_temp, &serial_fd);
    pthread_create(&t2, NULL, &compute_temp_stats, &max_seconds);
    pthread_create(&t3, NULL, &close_server, NULL);
    start_server(port_number);
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    
}
