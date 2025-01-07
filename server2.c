#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define PORT 2908
#define MAX_CLIENTI 100

// Variabile globale
int nr_clienti = 0, clienti[MAX_CLIENTI];
pthread_mutex_t mutex;
pthread_mutex_t db_mutex;
size_t WriteCallback(void *ptr, size_t size, size_t nmemb, char *data) {
    strcat(data, (char *)ptr);
    return size * nmemb;
}
// Structura thread-urilor
typedef struct thData {
    int idThread;
    int cl;
} thData;

void *treat(void *arg);
void raspunde(void *arg);

int login=0;
int benzinarii[] = {897, 779, 398, 534, 24, 781, 174, 860, 308, 124, 833, 781, 636, 26};
int main() {
    struct sockaddr_in server, from;
    int sd;

    // Inițializare mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Eroare la inițializarea mutex-ului");
        return errno;
    }
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
    perror("Eroare la inițializarea mutex-ului bazei de date");
    exit(EXIT_FAILURE);
}


    // Crearea socket-ului
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[server]Eroare la socket().");
        return errno;
    }

    // Configurare SO_REUSEADDR
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Configurare server
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    // Legarea socket-ului
    if (bind(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("[server]Eroare la bind().");
        return errno;
    }

    // Ascultarea conexiunilor
    if (listen(sd, 5) == -1) {
        perror("[server]Eroare la listen().");
        return errno;
    }

    printf("[server]Asteptam conexiuni la portul %d...\n", PORT);
    fflush(stdout);

    while (1) {
        int client;
        thData *td;
        int length = sizeof(from);

        // Acceptarea unui client
        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0) {
            perror("[server]Eroare la accept().");
            continue;
        }

        pthread_mutex_lock(&mutex);
        if (nr_clienti < MAX_CLIENTI) {
            clienti[nr_clienti++] = client;
        } else {
            printf("[server]Prea mulți clienți conectați! Refuz conexiunea.\n");
            close(client);
            pthread_mutex_unlock(&mutex);
            continue;
        }
        pthread_mutex_unlock(&mutex);

        td = (struct thData *) malloc(sizeof(struct thData));
        td->idThread = nr_clienti; // ID-ul este numărul clientului
        td->cl = client;

        pthread_t thread;
        pthread_create(&thread, NULL, &treat, td);
        pthread_detach(thread); // Thread-ul se va elibera automat la final
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}

void *treat(void *arg) {
    thData tdL = *((thData *) arg);
    printf("[thread %d]Conexiune nouă acceptată.\n", tdL.idThread);
    fflush(stdout);

    raspunde((void *) &tdL);

    close(tdL.cl);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < nr_clienti; i++) {
        if (clienti[i] == tdL.cl) {
            for (int j = i; j < nr_clienti - 1; j++) {
                clienti[j] = clienti[j + 1];
            }
            nr_clienti--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    free(arg);
    printf("[thread %d]Conexiune închisă.\n", tdL.idThread);
    return NULL;
}
void register_database(const char* username, const char* parola, char* stiri,thData tdL ) {
  
    sqlite3* db;
    char* zErrMsg = 0;
    const char* sql;
    int rc;

    rc = sqlite3_open("users.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    } else {
        fprintf(stdout, "Opened database successfully\n");
    }

    sql = "CREATE TABLE IF NOT EXISTS USERS ("
          "USERNAME TEXT NOT NULL,"
          "PASSWORD TEXT NOT NULL,"
          "NEWS TEXT);";

    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Table created successfully or already exists\n");
    }

    char insertSQL[512];
    char msg[100];
    snprintf(insertSQL, sizeof(insertSQL), 
             "INSERT INTO USERS (USERNAME, PASSWORD, NEWS) VALUES ('%s', '%s', '%s');", 
             username, parola, stiri);

    rc = sqlite3_exec(db, insertSQL, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        strcpy(msg,"eroare la sql");
        strcat(msg,"END");
         if (write(tdL.cl, msg, sizeof(msg)) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
        sqlite3_free(zErrMsg);
    } else {
        strcpy(msg,"user register successfully");
        strcat(msg,"END");
         if (write(tdL.cl, msg, sizeof(msg)) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
    }

    sqlite3_close(db);
}
void alerte_drum(thData tdL,char* nr)
{
   sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return ;
    }
    snprintf(sql,sizeof(sql),"SELECT smoothness from harta where ogr_fid='%s'",nr);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return;
    }
    rc = sqlite3_step(stmt);
    const char *raspuns= (const char *)sqlite3_column_text(stmt, 0);
    if(strncmp(raspuns,"bad",3)==0)
    {
        
         sqlite3_finalize(stmt);
         sqlite3_close(db);
         if (write(tdL.cl, "ATENTIE DRUM RAUEND!!",19) <= 0) {
            perror("[thread]Eroare la write().");
            return;}
    }
    else{
         sqlite3_finalize(stmt);
        sqlite3_close(db);}

}
void trimite_conexiuni(thData tdL,char* coordonat1)
{
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return ;
    }
   snprintf(sql, sizeof(sql), 
             "SELECT id_2 FROM connect WHERE id_1='%s';", 
             coordonat1);
rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return;
}
char msg[100]="Conexiune ";
while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *raspuns = (const char *)sqlite3_column_text(stmt, 0);
    strcat(msg,raspuns);
    strcat(msg," ");
   // printf("%s\n", raspuns);
}

if (rc != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
}
strcat(msg,"END");

sqlite3_finalize(stmt);
sqlite3_close(db);
if (write(tdL.cl,msg,strlen(msg)) <= 0) {
            perror("[thread]Eroare la write().");
            return;}

}
 void verificare_coordonate(char *coordonat1, thData tdL) {
    sqlite3* db;
    char* zErrMsg = 0;
    sqlite3_stmt* stmt;
    sqlite3_stmt* stmt2;
    char sql[200];
    char msg[100];
    int ok = 0;
    int accident=0; 
    int rc;

    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    } else {
        fprintf(stdout, "Opened database successfully\n");
    }

    snprintf(sql, sizeof(sql), "SELECT NAME FROM HARTA WHERE ogr_fid = '%s';", coordonat1);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
   
    rc = sqlite3_step(stmt);
    // printf("am trecut");
    if (rc == SQLITE_ROW) {
        ok = 1;
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        if (name) {
            strncpy(msg, name, sizeof(msg) - 1);  // Copy the result to 'msg'
            msg[sizeof(msg) - 1] = '\0';  // Ensure null-termination
        }
         /*if (write(tdL.cl, msg, sizeof(msg)) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }*/
    }

    if (!ok) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        snprintf(msg, sizeof(msg), "coordonat gresitEND"); 
        if (write(tdL.cl, msg, strlen(msg)) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
    
    }
    else{ 
    sqlite3_finalize(stmt);
    //snprintf(sql, sizeof(sql), "SELECT NAME FROM HARTA WHERE ogr_fid = '%s';", coordonat1);
    snprintf(sql,sizeof(sql),"SELECT accident from harta where ogr_fid='%s';",coordonat1);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
   
    rc = sqlite3_step(stmt2);
    if (rc == SQLITE_ROW) {
        const char *acc = (const char *)sqlite3_column_text(stmt2, 0);
        if(strncmp(acc,"da",2)==0)
        {   
             if (write(tdL.cl, "VITEZA REDUSA-ACCIDENT-mai este?EN1", 36) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
            accident=1;
             /*if (write(tdL.cl, "Este accident?", 15) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }*/
            char buf[500];
            int n = read(tdL.cl, buf, sizeof(buf) - 1);
            printf("accident %s\n",buf);
            sqlite3_finalize(stmt2);
            if (n <= 0) {
                printf("[thread %d]Client deconectat.\n", tdL.idThread);
                return;
            }
            if(strncmp(buf,"nu",2)==0)
                { 
                    accident=0;
                    snprintf(sql,sizeof(sql),"UPDATE harta set accident='%s' where ogr_fid='%s';","nu",coordonat1);
                   rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
                   if (rc != SQLITE_OK) {
                    strcpy(msg,"eroare la sql");
                    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
                    return;
                   sqlite3_free(zErrMsg);}
                   else {
                  printf("update accident cu succes");

                }
        }
     }   
     else{sqlite3_finalize(stmt2);
        sqlite3_close(db);}
     
    }
    else{sqlite3_finalize(stmt2);}
    sqlite3_close(db);
    sqlite3* db2;
    char sql2[200]; 
    char sql[200];
    int rc;
    int rc2;

    rc2 = sqlite3_open("export.sqlite", &db2);
    if (rc2) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db2));
        return;
    } else {
        fprintf(stdout, "Opened database successfully\n");
    }
    snprintf(sql, sizeof(sql), "SELECT MAXSPEED FROM HARTA WHERE ogr_fid='%s';",coordonat1);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    char viteza2[100];
    rc = sqlite3_step(stmt);
    const char *viteza= (const char *)sqlite3_column_text(stmt, 0);  
    if (viteza) {
            strncpy(viteza2, viteza, sizeof(viteza) - 1);  // Copy the result to 'msg'
            viteza2[sizeof(viteza2)] = '\0';  
        }
   // printf("viteza: %s",viteza2);
    sqlite3_finalize(stmt);
    snprintf(sql2, sizeof(sql2), "UPDATE MASINI SET STRADA='%s',VITEZA_STRADA='%s' WHERE ID='%d';",coordonat1,viteza2,tdL.idThread);
    rc2 = sqlite3_exec(db2, sql2, 0, 0, &zErrMsg);
    if (rc2 != SQLITE_OK) {
        strcpy(msg,"eroare la sql");
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db2));
            strcat(msg,"END");
         if (write(tdL.cl, msg, strlen(msg)) <= 0) {
                sqlite3_close(db2);
                perror("[thread]Eroare la write().");
                return;
            }
        sqlite3_free(zErrMsg);
    } else {
        if(strncmp(viteza2,"RO",2)==0)
        {
          strcpy(viteza2,"50");
        }
        char msg2[300];
        if(accident==0){
        snprintf(msg2,sizeof(msg2)," %s - VITEZA %s",msg,viteza2);}
        else{
           snprintf(msg2,sizeof(msg2)," %s - VITEZA REDUSA ACCIDENT",msg);
        }
        strcat(msg2,"END");
        printf("%s",msg2);
         if (write(tdL.cl, msg2, strlen(msg2)) <= 0) {
                perror("[thread]Eroare la write().");
                sqlite3_close(db2);
                return;
            }
        
             sqlite3_close(db2);
            alerte_drum(tdL,coordonat1);
            trimite_conexiuni(tdL,coordonat1);

    } 
 }
 }
 
void login_database(const char* username, const char* parola, thData tdL) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[512]; 
    char* zErrMsg = 0;
    const char *raspuns;
    rc = sqlite3_open("users.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    snprintf(sql, sizeof(sql),
             "SELECT USERNAME, PASSWORD FROM USERS WHERE USERNAME = '%s' AND PASSWORD = '%s';",
             username, parola);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        login = 1; 
    }
    sqlite3_finalize(stmt);
    snprintf(sql, sizeof(sql),
            "SELECT NEWS FROM USERS WHERE USERNAME = '%s' AND PASSWORD = '%s';",
             username, parola);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
     if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        raspuns = (const char *)sqlite3_column_text(stmt, 0); 
    }
    //memoram masina in baza de date
    sqlite3* db2;
    sqlite3_stmt* stmt2;
    int rc2;
    char sql2[200];
    rc2 = sqlite3_open("export.sqlite", &db2);
    if (rc2) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db2));
        return;
    }
    snprintf(sql2,sizeof(sql2), "CREATE TABLE IF NOT EXISTS MASINI ("
          "ID NUMBER NOT NULL,"
          "STRADA TEXT,"
          "VITEZA_STRADA TEXT,"
          "VITEZA NUMBER"
          "STIRI TEXT);");
    rc2 = sqlite3_exec(db2, sql2, 0, 0, &zErrMsg);
    if (rc2 != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Table created successfully or already exists\n");
    }
    printf("raspuns:'%s'\n",raspuns);
    char insertSQL[512];
    snprintf(insertSQL, sizeof(insertSQL), 
             "INSERT INTO MASINI (ID, STRADA, VITEZA_STRADA,VITEZA,STIRI) VALUES ('%d', '%p', '%p','%p','%p');", 
             tdL.idThread,NULL,NULL,NULL,NULL);
    rc2= sqlite3_exec(db2, insertSQL, 0, 0, &zErrMsg);
    if (rc2 != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } 
    snprintf(sql2, sizeof(sql2),
            "UPDATE MASINI SET STIRI='%s' WHERE ID='%d';",raspuns,
             tdL.idThread);
     rc2 = sqlite3_exec(db2, sql2, 0, 0, &zErrMsg);
    if (rc2 != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    char msg[100];
    if (login) {
        strcpy(msg,  "Login successful");
    } else {
        strcpy(msg,  "Login failed: Invalid username or password");
    }
    strcat(msg,"END");
    if (write(tdL.cl, msg, sizeof(msg)) <= 0) {
        perror("[thread] Error writing to client.");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    sqlite3_close(db2);
}
void inregistrare_viteza(int nr, thData tdL) {
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE MASINI SET VITEZA = %d WHERE ID = %d;", nr, tdL.idThread);

    rc = sqlite3_exec(db, sql,0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error during UPDATE: %s\n", zErrMsg);
        sqlite3_close(db);
        sqlite3_free(zErrMsg);
    } else{
         fprintf(stdout, "Transaction committed successfully.\n");
        
    }

    sqlite3_close(db);
}
void atentionare_viteza(int nr, thData tdL)
{
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;
    char sql[200];
    int viteza;
    sqlite3_stmt* stmt;
    // Deschide baza de date
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }
    snprintf(sql, sizeof(sql), "SELECT VITEZA_STRADA FROM masini WHERE id = '%d';",tdL.idThread );

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);  // Get the name column value
        if (name) {
            if(strncmp(name,"RO",2)==0)
              viteza=50;
            else viteza=atoi(name);
        }
        char msg[100];
        if(nr-viteza>10)
            strcpy(msg,"VITEZA PREA MARE!!!");
        else strcpy(msg,"viteza ok");
         strcat(msg,"END");
        if (write(tdL.cl, msg, strlen(msg)) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
    }
     sqlite3_finalize(stmt);

}
void sterge_db(thData tdL)
{
     char* zErrMsg = 0;
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }

     snprintf(sql, sizeof(sql), 
             "DELETE FROM MASINI WHERE (ID='%d');", 
             tdL.idThread);
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "STERGERE CU SUCCES\n");
    }
     sqlite3_close(db);
}
void anuntare_accident(int nr_clienti,int clienti[100],thData tdL)
{   char* zErrMsg = 0;
    sqlite3* db;
    sqlite3_stmt* stmt;
     sqlite3_stmt* stmt2;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
     if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    }
    snprintf(sql,sizeof(sql),"SELECT STRADA FROM MASINI WHERE ID='%d'",tdL.idThread);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    rc = sqlite3_step(stmt);
    if(rc==SQLITE_ROW)
    {
         const char *name = (const char *)sqlite3_column_text(stmt, 0);  
              printf("strada %s \n",name);
              snprintf(sql, sizeof(sql), "SELECT NAME FROM HARTA WHERE ogr_fid = '%s';", name);
         rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);
         if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
             sqlite3_close(db);
           return;
             }
         rc = sqlite3_step(stmt2);
         const char *name2 = (const char *)sqlite3_column_text(stmt2, 0); 
         char msg[100]="Accident pe ";
         strcat(msg,name2);
        snprintf(sql,sizeof(sql),"UPDATE harta SET accident='%s' where ogr_fid = '%s' ","da",name);
         sqlite3_finalize(stmt2);
         sqlite3_finalize(stmt);
         pthread_mutex_lock(&mutex);
            for (int i = 0; i < nr_clienti; i++) {
                strcat(msg,"END");
                if (write(clienti[i], msg, strlen(msg)) <= 0) {
                    printf("[thread %d]Eroare la trimiterea mesajului către clientul %d.\n", tdL.idThread, clienti[i]);
                } else {
                    printf("[thread %d]Mesaj transmis cu succes către clientul %d.\n", tdL.idThread, clienti[i]);
                }
            }
            pthread_mutex_unlock(&mutex);
          //  printf("%s",name);
            //snprintf(sql,sizeof(sql),"UPDATE harta SET accident='%s' where ogr_fid = '%s' ","da",name);
            rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
            if (rc != SQLITE_OK) {
             fprintf(stderr, "SQL error: %s\n", zErrMsg);
             sqlite3_free(zErrMsg);
            } else {
             fprintf(stdout, "INREGISTRARE ACCIDENT SUCCES\n");
    }
    }
    
      sqlite3_close(db);
          

}
int e_logat(thData tdL)
{

    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
     snprintf(sql, sizeof(sql), 
             "SELECT * FROM MASINI WHERE (ID='%d');", 
             tdL.idThread);
     rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
          sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 0;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
          sqlite3_finalize(stmt);
         sqlite3_close(db);
        return 1;
    }
     sqlite3_finalize(stmt);
     sqlite3_close(db);
    return 0;


}
void benzinarie(thData tdL)
{
    sqlite3* db;
    sqlite3_stmt* stmt;
     sqlite3_stmt* stmt2;
    int rc;
    char sql[100];
    int nr[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return ;
    }
    snprintf(sql,sizeof(sql),"SELECT strada from MASINI where id='%d'",tdL.idThread);
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return ;
    }
    rc = sqlite3_step(stmt);
    const char *strada = (const char *)sqlite3_column_text(stmt, 0);
    int street=atoi(strada);
    sqlite3_finalize(stmt);  
    int min[3],dif1=9999,dif2=9999,dif3=9999;
   for (int i = 0; i < 14; i++) {
        int dif = abs(street - benzinarii[i]);

        
        if (dif < dif1) {
            dif3 = dif2;
            min[3] = min[2];
            dif2 = dif1;
            min[2] = min[1];
            dif1 = dif;
            min[1] = benzinarii[i];
        }
        else if (dif < dif2) {
            dif3 = dif2;
            min[3] = min[2];
            dif2 = dif;
            min[2] = benzinarii[i];
        }
        else if (dif < dif3) {
            dif3 = dif;
            min[3] = benzinarii[i];
        }
    }
    char msg[500]="\0"; 


   for(int i=1;i<=3;i++){
        snprintf(sql,sizeof(sql),"SELECT name from benzinarii where strada='%d';",min[i]);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt2);
            sqlite3_close(db);
            return ;
    }
         rc = sqlite3_step(stmt2);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "Nu s a gasit %d\n", tdL.idThread);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return;
    } 
        const char *benz = (const char *)sqlite3_column_text(stmt2, 0);
        printf("%s",benz);
        strcat(msg,benz);
        strcat(msg,"--");
        sqlite3_finalize(stmt2);
        snprintf(sql,sizeof(sql),"SELECT name from harta where ogr_fid='%d';",min[i]);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt2);
            sqlite3_close(db);
            return ;
        }
        rc = sqlite3_step(stmt2);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "Nu s a gasit %d\n", tdL.idThread);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return;
    }
         const char *nume= (const char *)sqlite3_column_text(stmt2, 0);
        strcat(msg,nume);
        strcat(msg,"\n");
        sqlite3_finalize(stmt2);
        snprintf(sql,sizeof(sql),"SELECT preturi from benzinarii where strada='%d';",min[i]);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt2);
            sqlite3_close(db);
            return ;
    }
        rc = sqlite3_step(stmt2);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "Nu s a gasit %d\n", tdL.idThread);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return;
    }
        const char *detalii = (const char *)sqlite3_column_text(stmt2, 0);
        strcat(msg,detalii);
        sqlite3_finalize(stmt2);
        strcat(msg,"\n\n\n");
        
        }
        strcat(msg,"END");
         if (write(tdL.cl, msg,strlen(msg)) <= 0) {
            perror("[thread]Eroare la write().");
            return;}

}
int acces_stiri(thData tdL)
{
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    char sql[100];
    rc = sqlite3_open("export.sqlite", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
     snprintf(sql, sizeof(sql), 
             "SELECT stiri FROM MASINI WHERE (ID='%d');", 
             tdL.idThread);
     rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return 0;
    }
        rc = sqlite3_step(stmt);
        const char *raspuns= (const char *)sqlite3_column_text(stmt, 0);
        //printf("rapuuunss %s",raspuns);
        if(strncmp(raspuns,"nu",2)==0)
        { 
            printf("am intrat \n");
            sqlite3_finalize(stmt);
            if (write(tdL.cl,"Nu ai acces la optiunea cu stiriEND",36) <= 0) {
            perror("[thread]Eroare la write().");
            return 0;}
        }
        else
        {
             sqlite3_finalize(stmt);
             return 1;

        }
         


}

void sport(thData tdL)
{
    char stiri[] = "1. Fotbal - Campionatul European 2024: Nationala Romaniei continua pregatirile pentru Campionatul European. Selectia include tineri jucatori promitatori.\n"
               "2. Tenis - Australian Open: Simona Halep revine pe teren si castiga primele doua meciuri. Novak Djokovic isi apara titlul castigat anul trecut.\n"
               "3. Handbal - Campionatul Mondial Feminin: Romania invinge Norvegia si ajunge in sferturile de finala. Portarul Daria Ciobanu a avut o evolutie exceptionala.\n"
               "4. Baschet - NBA: LeBron James depaseste 40.000 de puncte marcate in cariera si spera la un nou titlu cu Los Angeles Lakers.\n"
               "5. Motorsport - Formula 1: Max Verstappen, campionul en-titre, este favorit in noul sezon de Formula 1, dar Hamilton si Leclerc promit rivalitate puternica.";
    strcat(stiri,"END");
     if(write(tdL.cl,stiri,strlen(stiri))<=0)
     {
        perror("[thread]Eroare la write().");
            return;}

     
}
void afiseazaVremea(const char *json_response,thData tdL) {
    // Parsează răspunsul JSON
    struct json_object *parsed_json;
    struct json_object *current_weather;
    struct json_object *temperature;
    struct json_object *windspeed;
    struct json_object *weathercode;

    char afisare[600] = "";

    parsed_json = json_tokener_parse(json_response);

    // Accesează datele 
    json_object_object_get_ex(parsed_json, "current_weather", &current_weather);
    json_object_object_get_ex(current_weather, "temperature", &temperature);
    json_object_object_get_ex(current_weather, "windspeed", &windspeed);
    json_object_object_get_ex(current_weather, "weathercode", &weathercode);

    // Extrage informațiile
    double temp = json_object_get_double(temperature);
    double wind = json_object_get_double(windspeed);
    int code = json_object_get_int(weathercode);


    snprintf(afisare, sizeof(afisare),
             "\n===== Vremea în Iași =====\n"
             "Temperatura: %.1f °C\n"
             "Viteza vântului: %.1f km/h\n"
             "Condiții meteo: ", temp, wind);

    // Traducere cod meteo și adăugare la mesaj
    switch (code) {
        case 0: strncat(afisare, "Cer senin\n", sizeof(afisare) - strlen(afisare) - 1); break;
        case 1: case 2: case 3: strncat(afisare, "Parțial înnorat\n", sizeof(afisare) - strlen(afisare) - 1); break;
        case 45: case 48: strncat(afisare, "Ceață\n", sizeof(afisare) - strlen(afisare) - 1); break;
        case 51: case 53: case 55: strncat(afisare, "Burniță\n", sizeof(afisare) - strlen(afisare) - 1); break;
        case 61: case 63: case 65: strncat(afisare, "Ploaie\n", sizeof(afisare) - strlen(afisare) - 1); break;
        case 71: case 73: case 75: strncat(afisare, "Ninsoare\n", sizeof(afisare) - strlen(afisare) - 1); break;
        default: strncat(afisare, "Condiții necunoscute\n", sizeof(afisare) - strlen(afisare) - 1); break;
    }
    strcat(afisare,"END");
    if (write(tdL.cl,afisare,strlen(afisare)) <= 0) {
            perror("[thread]Eroare la write().");
            return ;}
    printf("%s", afisare);
    json_object_put(parsed_json);
}

void vreme(thData tdL)
{
    CURL *curl;
    CURLcode res;
    char response[10000] = ""; 

    curl = curl_easy_init();
    if (curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.open-meteo.com/v1/forecast?latitude=47.16&longitude=27.58&current_weather=true");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
         
            afiseazaVremea(response,tdL);
        }

        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Eroare la inițializarea CURL\n");
    }
}
void menu_page(thData tdL)
{
    char menu[600] = "\n========================================\n       -MONITORIZAREA TRAFICULUI-       \n========================================\n\nComenzi disponibile:\n========================================\n1. Logare:                [login 'nume' 'parola']\n2. Inregistrare:          [register 'nume' 'parola' 'stiri(da/nu)']\n3. Logout:                [logout]\n4. Raportare accident:    ['accident']\n5. Benzinarii apropiate:  [benzinarie]\n6. Informatii meteo: ['vreme']\n7 .Stiri despre sport: ['sport']\n\n========================================\n\n\n";
    strcat(menu,"END");
  //  printf("%s",menu);
      if (write(tdL.cl, menu,strlen(menu)) <= 0) {
            perror("[thread]Eroare la write().");
            return;}

}

void raspunde(void *arg) {
    thData tdL = *((thData *) arg);//converteste pointerul generic la un pointer de tip thData
    char buf[256];

    while (1) {
        bzero(buf, 256);
        int n = read(tdL.cl, buf, sizeof(buf) - 1);
        if (n <= 0) {
            sterge_db(tdL);
            printf("[thread %d]Client deconectat.\n", tdL.idThread);
            return;
        }

        buf[n] = '\0';
        printf("[thread %d]Mesaj primit: %s\n", tdL.idThread, buf);

        if (strncmp(buf, "accident", 8) == 0) {
            if(e_logat(tdL)==0)
            {
                 
                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
            }
            else{
                anuntare_accident(nr_clienti,clienti,tdL);
            }
        } else if (strncmp(buf, "exit", 4) == 0) {
            printf("[thread %d]Client a cerut deconectare.\n", tdL.idThread);
            sterge_db(tdL);
            return;
        } else if(strncmp(buf,"register",8)==0)
        {
        
            char username[50];
            char password[50];
            char stiri[50];

             char* p = strtok(buf, " ");
             p = strtok(NULL, " ");
            if (p == NULL) {
                fprintf(stderr, "Invalid input format.\n");
                return ;
            }
            strcpy(username, p);

            p = strtok(NULL, " ");
            if (p == NULL) {
            fprintf(stderr, "Invalid input format.\n");
            return ;
            }
            strcpy(password, p);

            p = strtok(NULL, " ");
                if (p == NULL) {
                    fprintf(stderr, "Invalid input format.\n");
                    return ;
                 }
         strcpy(stiri, p);
        register_database(username, password, stiri,tdL);
        }
        else if (strncmp(buf,"login",5)==0)
        { 

            //printf("am intrat");
            
             char username[50];
             char password[50];
            char* p = strtok(buf, " ");
            p = strtok(NULL, " ");
            if (p == NULL) {
                fprintf(stderr, "Invalid input format.\n");
                return ;
            }
            strcpy(username, p);

            p = strtok(NULL, " ");
            if (p == NULL) {
            fprintf(stderr, "Invalid input format.\n");
            return ;
            }
            strcpy(password, p);
            login_database(username,password,tdL);
        }
        else if(strncmp(buf,"logout",6)==0)
                {
                sterge_db(tdL);
                if (write(tdL.cl, "logout sucessfullyEND",22) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
                }
      
        else if(strncmp(buf,"coordonate",10)==0)
        {
            if(e_logat(tdL)==0)
            {

                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
            }
            else {
                char* p=strtok(buf," ");
                p=strtok(NULL," ");
                char coordonat1[20];
                strcpy(coordonat1,p);
                verificare_coordonate(coordonat1,tdL);
            }
        
        }else if(strncmp(buf,"viteza",6)==0){
            if(e_logat(tdL)==0)
                 {

                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
                }
            }
            else 
                {
                    char *p=strtok(buf," ");
                    p=strtok(NULL," ");
                    int nr;
                    nr=atoi(p);
                    inregistrare_viteza(nr,tdL);
                    atentionare_viteza(nr,tdL);
                }
        }
        else if(strncmp(buf,"benzinarie",10)==0){
           // printf("am intrat");
            if(e_logat(tdL)==0)
                 {

                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
                }
            }
            else 
                {
                    if(acces_stiri(tdL)==1)
                    {
                        benzinarie(tdL);
                    }
                    else
                    if (write(tdL.cl, "Nu ai access la aceste optiuniEND",34) <= 0) {
                        perror("[thread]Eroare la write().");
                        return;
                     }
                    
                }
        }
        else if(strncmp(buf,"menu",4)==0)
        { 
            printf("aicii");
            menu_page(tdL);
        }
        else if(strncmp(buf,"vreme",5)==0)
        {
             if(e_logat(tdL)==0)
                 {

                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
                }
            }
            else 
                {
                    if(acces_stiri(tdL)==1)
                    {
                       vreme(tdL);
                    }
                }
        }
        else if(strncmp(buf,"sport",5)==0)
        {  printf("alooo");
            if(e_logat(tdL)==0)
                 {

                 if (write(tdL.cl, "no permissionEND",16) <= 0) {
                perror("[thread]Eroare la write().");
                return;
                }
            }
            else 
                {
                    if(acces_stiri(tdL)==1)
                    {
                       sport(tdL);
                    }
                }
        }
         else{
            if (write(tdL.cl, buf, n) <= 0) {
                perror("[thread]Eroare la write().");
                return;
            }
        }
}
}

