#define LOGIN_CMD "/login"
#define LOGOUT_CMD "/logout"
#define JOIN_CMD "/joinsession"
#define LEAVE_CMD "/leavesession"
#define CREATE_CMD "/createsession"
#define LIST_CMD "/list"
#define QUIT_CMD "/quit"


    if (cur_server == NULL){
        fprintf(stdout, "Server not avaliable!\n");
        return;
    }
    
    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    packet pack;
    pack.type = LOGIN;
    strncpy(pack.source, id, MAX_SOURCE_SIZE);
    strncpy(pack.data, password, MAX_DATA_SIZE);
    pack.size = strlen(pack.data);

    StrToPack(&pack, buffer);
    if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: login message send error\n");
        close(*socketfd);
        return;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: login message receive error\n");
        close(*socketfd);
        return;
    }
    
    PackToStr(&pack, buffer);
    if (pack.type == 2){ // LO_ACK
        if (pthread_create(thread, NULL, client_func, (void *)socketfd) == 0)
        fprintf(stdout, "Login Successfully.\n");
    }
    else if (pack.type == 3){ // LO_NAK
        fprintf(stdout, "ERROR: client login - receiving NAK. \n");
        fprintf(stdout, pack.data);
        connected = false;
        close(*socketfd);
        return;
    }
    else {
        fprintf(stdout, "ERROR: client login - receiving unexpected packet type. \n");
        connected = false;
        close(*socketfd);
        return;
    }