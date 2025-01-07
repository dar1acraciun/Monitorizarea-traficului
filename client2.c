#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <time.h> // Pentru srand() și time()
#include <unistd.h> // Pentru getpid()
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server */
int port;
int sd; // Descriptorul socket-ului clientului
int is_logged_in = 0;
int coord[5],nr_cord=0;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_lock = PTHREAD_MUTEX_INITIALIZER;

void *read_from_server(void *arg) {
    char buf[500];
    char message[1024] = {0}; // Buffer pentru mesaj complet
    int msg_index = 0;
    write(sd,"menu",5);
     while (1) {
       // printf("%d \n",is_logged_in);
        bzero(buf, sizeof(buf));
        int n = read(sd, buf, sizeof(buf) - 1);
      //  printf("%d",n);
        if (n <= 0) {
            perror("[client] Eroare la read() de la server.\n");
            break;
        }
        for (int i = 0; i < n; i++) {
            message[msg_index++] = buf[i];
            //printf("%s\n",message);
            // Detectează markerul de sfârșit "END"
            
              if (strstr(message, "EN1")) {
               message[msg_index - 3] = '\0'; // Elimină "EN1"
               printf("\n[server]: %s\n", message);
                msg_index = 0;
                bzero(message, sizeof(message));
                break; // Ieșim din buclă pentru a preveni procesarea reziduală
            }
            else if (msg_index >= 3 && strncmp(message + msg_index - 3, "END", 3) == 0) {
             //  printf("%s",message);
                message[msg_index - 3] = '\0'; // Elimină markerul "END"
                 if (strncmp(message, "logout sucessfully", 19) == 0) {
                    printf("\n[server]: %s\n", message);
                    write(sd,"menu",5);
                    is_logged_in = 0;  
                    msg_index = 0;
                    bzero(message, sizeof(message)); 
                    break;
                
                }
                if (strncmp(message, "Login successful", 16) == 0) {
                    printf("\n[server]: %s\n", message);
                    is_logged_in = 1; 
                    msg_index = 0;
                    bzero(message, sizeof(message)); 
                    break;
                }
                  if(strncmp(message,"coordonat gresit",16)==0)
            { 
                 printf("\n[server]: %s\n", message);
                int rd_num = rand() % (926 - 15 + 1) + 10;
                sprintf(buf, "%d", rd_num);
                char buf2[100];
                 strcpy(buf2,"coordonate ");
                 strcat(buf2,buf);
                 strcpy(buf,buf2);
                printf("coordonate trimise: %s\n", buf);
                 buf[strcspn(buf, "\r\n")] = '\0';
                 buf[strcspn(buf, "\n")] = '\0';

                 if (write(sd, buf, strlen(buf)) <= 0) {
                 perror("[client] Eroare la write().\n");
                msg_index = 0;
                bzero(message, sizeof(message)); 
                 break;
                  }

            }
           else if(strncmp(message,"Conexiune",9)==0)
            {
                 char* p;
                 nr_cord = 0;
                 p = strtok(message, " ");
                 while (p)
                 {   
              //   printf("%d\n", nr_cord);
                coord[nr_cord] = atoi(p);  // Fără a incrementa nr_cord înainte
                nr_cord++;  // Incrementăm nr_cord doar după ce adăugăm valoarea
                 p = strtok(NULL, " ");
          }
           // printf("numarul de coord este %d", nr_cord);
            nr_cord--;
             msg_index = 0;
             bzero(message, sizeof(message)); 
             break;
            }

                msg_index = 0; // Resetează indexul pentru următorul mesaj
                bzero(message, sizeof(message)); // Resetează bufferul de mesaj
            }
        }
       
    
        
        printf("[client] Introduceti un mesaj: ");
        fflush(stdout);}
    
    pthread_exit(NULL);
}

void *send_cord(void* arg)
{
    char buf[256];
    int rd_num;
    nr_cord=1;
    rd_num = rand() % (926 - 15 + 1) + 10;
    coord[nr_cord]=rd_num;
    while (!is_logged_in) {}
    while (is_logged_in) {
         // Trimite datele la fiecare 60 de secunde

        // Generează un număr aleator între 10 și 200
       rd_num = rand() % nr_cord + 1;
      // printf("nr de coordonate este:%d\n",nr_cord);
      //printf("nr random este %d\n",rd_num);
       sprintf(buf, "%d", coord[rd_num]);
        char buf2[100];
        strcpy(buf2,"coordonate ");
        strcat(buf2,buf);
        strcpy(buf,buf2);
        printf("coordonate trimise: %s\n", buf);
        buf[strcspn(buf, "\r\n")] = '\0';
        buf[strcspn(buf, "\n")] = '\0';

        if (write(sd, buf, strlen(buf)) <= 0) {
            perror("[client] Eroare la write().\n");
            break;
        }
        for (int i = 0; i < 60 && is_logged_in; i += 1) {
        sleep(1);
        }
    
        while (!is_logged_in) {
            rd_num = rand() % (926 - 15 + 1) + 10;
        }
    
    }
    pthread_exit(NULL);
}

void *send_speed(void *arg) {
    char buf[256];
    int speed = 50; // Viteza inițială
    int max_speed = 130; // Viteza maximă
    int min_speed = 0; // Viteza minimă
    int acceleration = 15; // Accelerație mai mare
    int deceleration = -5; // Decelerație mai mică

    while (!is_logged_in) {
        // Așteaptă să fie logat
    }

    while (is_logged_in) {
        // Decide dacă accelerăm sau decelerăm
        int change = (rand() % (acceleration - deceleration + 1)) + deceleration;

        // Aplicăm schimbarea și verificăm limitele
        speed += change;

        // Asigură-te că viteza rămâne în intervalul valid
        if (speed > max_speed) speed = max_speed;
        if (speed < min_speed) speed = min_speed;

        // Pregătim mesajul
        sprintf(buf, "viteza %d", speed);
        printf("viteza trimisă: %s\n", buf);

        // Trimitem viteza
        buf[strcspn(buf, "\r\n")] = '\0';
        if (write(sd, buf, strlen(buf)) <= 0) {
            perror("[client] Eroare la write().\n");
            break;
        }

        for (int i = 0; i < 60 && is_logged_in; i += 1) {
             sleep(1);
        }
    
           while (!is_logged_in) {}
    
    }

    pthread_exit(NULL);
}



void *write_to_server(void *arg)
{ 
    char buf[256];
    while (1) {
        fflush(stdout);

        bzero(buf, 256);
        int n = read(0, buf, sizeof(buf));
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        buf[strcspn(buf, "\n")] = '\0';

        if (strcmp(buf, "exit") == 0) {
            printf("[client] Închidem conexiunea...\n");
            write(sd, buf, strlen(buf));
            break;
        }

        if (write(sd, buf, strlen(buf)) <= 0) {
            perror("[client] Eroare la write().\n");
            break;
        }
           while (!is_logged_in) {}
    }

}


int main(int argc, char *argv[]) {
    struct sockaddr_in server;

    if (argc != 3) {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("[client] Eroare la connect().\n");
        return errno;
    }

    
    // Setăm sămânța pentru generatorul de numere aleatoare
    srand(time(NULL) + getpid());

    pthread_t read_thread, thread_viteza,thread_coord,thread_write;
    pthread_create(&read_thread, NULL, read_from_server, NULL);
    pthread_create(&thread_viteza, NULL, send_speed, NULL);
    pthread_create(&thread_coord,NULL,send_cord,NULL);
    pthread_create(&thread_write,NULL,write_to_server,NULL);

    pthread_join(read_thread, NULL);
    pthread_join(thread_write, NULL);
    pthread_join(thread_coord, NULL);
    pthread_join(thread_viteza, NULL); // Așteptăm terminarea thread-urilor
    close(sd);
    return 0;
}