#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

//message structure for message to a player
struct toPlayer{
    //indicated action to player
    enum{ok, getAddress, getWho} messageType;

    //list of logged on playerIDs
    int loggedIn [10];

    //requested ID number
    unsigned int requestID;

    //requested player's receiving port
    unsigned short int requestPort;

    //requested player's address
    unsigned long int requestAddress;
}
//initialize message sender
sendPlayer,

//initialize list of logged in players
activePlayers[10];

//initialize number of players logged in
unsigned int playerCount;

//message structure for message from a player
struct fromPlayer{
    //indicated action from the player
    enum{login, logout, askWho, askAddress} messageType;

    //playerID to perform the action on
    unsigned int playerID;

    //port number to act on
    unsigned short int playerPort;
}
//initialize message receiver
recvPlayer;

/**
 * Retreives logged in users and puts them into the sendPlayer message
 */
void getLogged(){
    for(int i = 0; i < playerCount; ++i)
        sendPlayer.loggedIn[i] = activePlayers[i].requestID;
}

/**
 * Finds the indicated ID number in the list of logged in players
 *
 * param: id    ID number of the player to find
 * return:      Index number of the ID if one given
 */
int findID(int id){
    //scan through logged in players
    for(int i = 0; i < playerCount; ++i){
        //return index of ID if found
        if(activePlayers[i].requestID == id)
            return i;
    }

    //return not found value
    return -1;
}

/**
 * Determines the next available ID number for a player
 *
 * return:      Available ID number
 */
int getNextID(){
    //initialize potential outputs
    int idIndex;
    int i;

    //for each potential existing ID number
    for(i = 1; i < playerCount + 1; ++i){
        //return guessed ID number if it doesn't exist
        if((idIndex = findID(i)) < 0)
            break;
    }

    //return highest possible value
    return i;
}

/**
 * Removes a player from the logged in list
 *
 * param: idNum     PlayerID to be removed from the list
 */
void logoutID(int idNum){
    //starting at the indicated ID number
    for(int i = findID(idNum); i < playerCount - 1; ++i){
        //move the next player item into this space
        activePlayers[i].requestID = activePlayers[i + 1].requestID;
        activePlayers[i].requestPort = activePlayers[i + 1].requestPort;
        activePlayers[i].requestAddress = activePlayers[i + 1].requestAddress;
    }

    //cut off the last item
    --playerCount;
}






int main(int argc, char * argv[]){
    //initialize receiving socket identifier
    int sock;

    //initialize own address parameters
    struct sockaddr_in servAddr;
    unsigned short servPort;

    //initialize contact address parameters
    struct sockaddr_in playAddr;
    int playAddrLen = sizeof(playAddr);

    //initialize message lengths
    unsigned int recvLen;
    unsigned int sendLen;

    //initialize structure sizes
    unsigned int recvStructSize = sizeof(struct fromPlayer);
    unsigned int sendStructSize = sizeof(struct toPlayer);

    //check if appropriate number of arguments used
    if(argc != 2){
        //notify user of action failure
        fprintf(stderr, "Usage: %s <UDP SERVER PORT>\n", argv[0]);
        exit(1);
    }

    //notify user of startup
    printf("Booting game server...\n");

    //set key manager's port
    servPort = atoi(argv[1]);

    //create socket
    if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        fprintf(stderr, "socket() failed\n");
        exit(1);
    }

    //establish port parameters
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);

    //bind socket
    if(bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        perror("bind() failed");
        exit(1);
    }

    //initialize logged in player counter
    playerCount = 0;

    for(;;){
        //label to exit failed processes
        top:

        //clear message items
        memset(&sendPlayer, 0, sizeof(sendPlayer));
        memset(&recvPlayer, 0, sizeof(recvPlayer));

        //notify user of ready state
        printf("\nReady and waiting for message...\n");

        //receive message from a player
        if((recvLen = recvfrom(sock, &recvPlayer, recvStructSize, 0, (struct sockaddr *) &playAddr, &playAddrLen)) < 0){
            printf("\nBad message received!\n");
            goto top;
        }

        //notify of message received
        printf("\nMessage received from player: ");

        //player is logging in to the server
        if(recvPlayer.messageType == login){
            //notify of player logging in
            printf("Logging in new player...\n");

            sendPlayer.messageType = getWho;

            //check if space available for new player
            if(playerCount >= 10){
                printf("Server is full! Cannot log player in.\n");

                //change port value to failed state
                sendPlayer.requestPort = 0;
            }

            else{
                //copy received data into active player list
                activePlayers[playerCount].requestID = getNextID();
                activePlayers[playerCount].requestPort = recvPlayer.playerPort;
                activePlayers[playerCount].requestAddress = playAddr.sin_addr.s_addr;

                //copy player stats back to sending message
                sendPlayer.requestID = activePlayers[playerCount].requestID;
                sendPlayer.requestPort = activePlayers[playerCount].requestPort;
                sendPlayer.requestAddress = activePlayers[playerCount].requestAddress;

                //increment number of players online
                ++playerCount;

                //set logged in players list
                getLogged();
            }

            //set value of message length
            sendLen = sizeof(sendPlayer);

            //send acknowledgement back to caller
            if(sendto(sock, &sendPlayer, sendLen, 0, (struct sockaddr *) &playAddr, playAddrLen) < 0)
                perror("Login acknowledgement failed");
        }

        //player is requesting a list of logged in users
        else if(recvPlayer.messageType == askWho){
            //notify of logged in check
            printf("Requesting active player list...\n");

            //set message type
            sendPlayer.messageType = getWho;

            //fill message logged in data
            getLogged();

            //send message back to caller
            if(sendto(sock, &sendPlayer, sendLen, 0, (struct sockaddr *) &playAddr, playAddrLen) < 0)
                perror("Who request failed");            
        }

        //player wishes to initiate a game with another user
        else if(recvPlayer.messageType == askAddress){
            //notify user of action
            printf("Connecting players for a game...\n");

            //get the requested player's information
            int targetIndex = findID(recvPlayer.playerID);

            //set the message type
            sendPlayer.messageType = getAddress;

            //set parameters for player not found result
            if(targetIndex < 0){
                sendPlayer.requestPort = 0;
                sendPlayer.requestAddress = 0;
            }

            //prepare player's port and address
            else{
                sendPlayer.requestPort = activePlayers[targetIndex].requestPort;
                sendPlayer.requestAddress = activePlayers[targetIndex].requestAddress;
            }
            
            //send message back to caller
            if(sendto(sock, &sendPlayer, sendLen, 0, (struct sockaddr *) &playAddr, playAddrLen) < 0)
                perror("Address request failed");               
        }

        //player is logging out of the active list
        else if(recvPlayer.messageType == logout){
            //notify of logout action taken
            printf("Logging player out...\n");

            //log indicated player out
            logoutID(recvPlayer.playerID);

            //set message type
            sendPlayer.messageType = ok;
            
            //send confirmation message to player
            if(sendto(sock, &sendPlayer, sendLen, 0, (struct sockaddr *) &playAddr, playAddrLen) < 0)
                perror("Player logout failed");              
        }

        //catch invalid action
        else
            printf("Message received has no action.\n");
    }
}