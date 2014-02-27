#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "protocol.h"

#define MAX_PLAYERS 30

//PLAYER
typedef struct player
{
	//client's PID == key for client's queue and shmget
	int pid;
	//client's queue ID (queue handler, not key)
	int clientQ;
	//level of player
	int skill;
	//color of chess pieces
	int color;
	//nick of player
	char name[40];
} player;

//GAME
typedef struct game 
{
	//game id == clientQ, handler for client queue (not key!)
	int gameID;
	//game queue ID
	int gameQ;
	//flag: 0 - game is empty, 1 - one player waiting for opponent, 2 - game is on, 3 - game finished
	int inProgress;
	//host's info
	player host;
	//opponent's info
	player secondPlayer;
} game;

//global arrays for players and games - both of max size == MAX_PLAYERS
game gameTable[MAX_PLAYERS];
player playerTable[MAX_PLAYERS];


//returns a handler for global queue
int openGlobalQ()
{
	int queueID;
	printf("> Global queue ID: ");
	scanf("%d", &queueID);
	int created = msgget(queueID, IPC_CREAT | 0666);
	if(created == -1) 
	{
		perror("Queue error!");
		return -1;
	}
	else 
		return created;
	
}

int openClientQ(int queueID)
{
	int created = msgget(queueID, IPC_CREAT | 0666);
	if(created == -1)
	{
		perror("Cannot open client's queue!");
		return -1;
	}
	else
		return created;
}

struct clientShMemory* readPreferences(int clientQ)
{
	struct clientShMemory *cShMemory;
	int cSharedMemId = shmget(clientQ, sizeof(struct clientShMemory), 0666);
	if(cSharedMemId == -1)
		perror("shmget error!");
	if((cShMemory = (struct clientShMemory*)shmat(cSharedMemId, NULL, 0)) == NULL)
		perror("shmat error!");

	return cShMemory;
}



int registerPlayer(int queueID)
{
	struct clientLogin cLogin;
	struct serverConfirmLoginResponse sConfirmLoginResponse;
		sConfirmLoginResponse.type = 2;
	int clientQ;

	//check if there are any requests for logging in
	if(msgrcv(queueID, &cLogin, sizeof(cLogin) - sizeof(long), 1, IPC_NOWAIT) != -1)
	{
		printf("> Request received! \n\t> nick: %s\n", cLogin.name);
		//czytaj preferencje gracza
		struct clientShMemory *cPreferences;
		cPreferences = readPreferences(cLogin.msgQueue);
		printf("\t> color: %d \n\t> level: %d\n-----\n", (*cPreferences).color, (*cPreferences).skill);

		printf("> Client queue ID: %d\n", cLogin.msgQueue);
		//open player's queue
		clientQ = openClientQ(cLogin.msgQueue);

		//add player to global array and save his preferences
		int i = 0;
		for(i = 0; i < MAX_PLAYERS; i++)
		{
			if(playerTable[i].clientQ == 0)
			{
				playerTable[i].pid = queueID;
				playerTable[i].clientQ = clientQ;
				playerTable[i].skill = (*cPreferences).skill;
				playerTable[i].color = (*cPreferences).color;
				strcpy(playerTable[i].name, cLogin.name);
				sConfirmLoginResponse.status = MSG_OK;
				break;
			}
		}
		//cannot add a player, because there are MAX_PLAYERS registered already
		if(i == MAX_PLAYERS)
			sConfirmLoginResponse.status = MSG_FULL;	
	}
	else
	{
		sConfirmLoginResponse.status = MSG_FAILED;
		return -1;
	}

	//send register confirmation -- with msg status
	if(msgsnd(clientQ, &sConfirmLoginResponse, sizeof(struct serverConfirmLoginResponse) - sizeof(long), 0) != -1)
		printf("> Register confirmation sent!\n");
	return clientQ;
}

//returns a new game queue's HANDLER (not key)
int createGame(int clientQ)
{
	struct serverCreateGameResponse sResponse;
		sResponse.type = 14;

	printf("> Received new game creation request.\n");
	//add player to a new game
	int i = 0;
	for(i = 0; i < MAX_PLAYERS; i++)
	{
		//first empty slot in global array
		if(gameTable[i].gameQ == 0)
		{
			gameTable[i].gameID = clientQ;
			int j = 0;
			for(j = 0; j < MAX_PLAYERS; j++)
				//find player's info
				if(playerTable[j].clientQ == clientQ)
					break;
			//add player's info to a new game
			srand(time(NULL));
			gameTable[i].gameID = getpid() ^ (rand() % 10000); // XOR in order to create an unique key
			gameTable[i].host.pid = playerTable[j].pid;
			gameTable[i].host.clientQ = playerTable[j].clientQ;
			gameTable[i].host.skill = playerTable[j].skill;
			gameTable[i].host.color = playerTable[j].color;
			gameTable[i].inProgress = 1;
			strcpy(gameTable[i].host.name, playerTable[j].name);
			strcpy(gameTable[i].secondPlayer.name, "");
			sResponse.status = MSG_OK;
			sResponse.gameQueue = gameTable[i].gameID;
			break; // <-- empty slot found
		}
	}
	if(msgsnd(clientQ, &sResponse, sizeof(struct serverCreateGameResponse) - sizeof(long), 0) != -1)
	{
		printf("> 'Game creation' confirmation sent!\n");
		printf("\t> Game queue: %d\n", sResponse.gameQueue);
	}
	int gameQ;
	if((gameQ = msgget(sResponse.gameQueue, IPC_CREAT | 0666)) != -1)
	{
		printf("> Game queue opened.\n");
		gameTable[i].gameQ = gameQ;
		return gameQ;
	}
	else
		return -1;	
}

void listGames(int clientQ)
{
	struct serverListGamesResponse sListGamesResponse;
		sListGamesResponse.type = 7;

	struct serverGameInfo sGameInfo;
		sGameInfo.type = 8;

	printf("> 'List games' request received.\n");
	int i = 0, counter = 0;

	//count existing games
	for(i = 0; i < MAX_PLAYERS; i++)
		if(gameTable[i].gameID != 0 && gameTable[i].inProgress != 2)
			counter++; // <-- number of existing games: filtering by player's preferences is on client's side
								//does not count full games

	
	sListGamesResponse.games = counter;
	if(msgsnd(clientQ, &sListGamesResponse, sizeof(struct serverListGamesResponse) - sizeof(long), 0) != -1)
	{
		printf("> Msg sent. Number of existing games = %d\n", sListGamesResponse.games);
		//send a game info as many times as necessary
		for(i = 0; i < MAX_PLAYERS; i++)
		{
			//find an existing game and save its info
			if(gameTable[i].gameID != 0 && gameTable[i].inProgress != 2)
			{
				sGameInfo.gameID = gameTable[i].gameID;
				sGameInfo.skill = gameTable[i].host.skill;
				sGameInfo.color = gameTable[i].host.color;
				strcpy(sGameInfo.players[0], gameTable[i].host.name);
				strcpy(sGameInfo.players[1], gameTable[i].secondPlayer.name);
				printf("> Sending game:\n\t>>> %d, level: %d, color: %d, host: %s\n", sGameInfo.gameID, sGameInfo.skill, sGameInfo.color, sGameInfo.players[0]);
				//po znalezieniu istniejacej gry, przeslij jej dane do klienta, ktory o te dane poprosil
				if(msgsnd(clientQ, &sGameInfo, sizeof(struct serverGameInfo) - sizeof(long), 0) != -1)
				{
					printf("\t >>> Sent info about game with ID = %d\n", sGameInfo.gameID);
				}
				else
					printf("> Cannot send info about game with ID = %d\n", sGameInfo.gameID);
			}
		}
	}
	else
		printf("> Cannot send confirmation.\n");
}

//add player to existing game
void matchPlayer(int clientQ, int gameID)
{
	struct serverJoinGameResponse sJoinGameResponse;
		sJoinGameResponse.type = 10;
	int i;
	for(i = 0; i < MAX_PLAYERS; i++)
	{
		if(gameTable[i].gameID == gameID && gameTable[i].inProgress == 1)
		{
			sJoinGameResponse.status = MSG_OK;
			strcpy(sJoinGameResponse.name, gameTable[i].host.name);
			sJoinGameResponse.color = gameTable[i].host.color;
			sJoinGameResponse.gameQueue = gameTable[i].gameID;
			break;
		}
		else
			sJoinGameResponse.status = MSG_FAILED;
	}
	if(i == MAX_PLAYERS)
	{
		printf("> Cannot join a game. Game not found!\n");
	}
	//game found and info saved
	if(msgsnd(clientQ, &sJoinGameResponse, sizeof(struct serverJoinGameResponse) - sizeof(long), 0) != -1)
	{
		printf("> 'Join game' confirmation sent.\n");
		//add a new player to global array of players
		player currentPlayer;
		int j;
		for(j = 0; j < MAX_PLAYERS; j++)
			if(playerTable[j].clientQ == clientQ)
				break;
		currentPlayer = playerTable[j];
		gameTable[i].secondPlayer = currentPlayer;
		gameTable[i].inProgress = 2; // opponent joined
		//send a permission to start a game
		struct serverNotifyClient sNotifyClient;
			sNotifyClient.type = 11;
		strcpy(sNotifyClient.name, gameTable[i].secondPlayer.name);
		if(msgsnd(gameTable[i].host.clientQ, &sNotifyClient, sizeof(struct serverNotifyClient) - sizeof(long), 0) != -1)
		{
			printf("> Sent a permission to host!\n");
		}
		if(msgsnd(gameTable[i].secondPlayer.clientQ, &sNotifyClient, sizeof(struct serverNotifyClient) - sizeof(long), 0) != -1)
		{
			printf("> Sent a permission to opponent!\n");
		}
	}
	else
		printf("> Cannot send a confirmation.\n");

}

//serve one client at a time -- wait for any kind of requests
void waitForRequest(int clientQ)
{
	//client's possible requests
	struct clientCreateGame cCreateGame;	//create a new game 		-> Id = 5
		cCreateGame.type = 5;

	struct clientCreateGame cListGames;	//list all games 				-> Id = 6
		cListGames.type = 6;
	
	struct clientJoinGame cJoinGame;	//join an existing game     	-> Id = 9
		cJoinGame.type = 9;

	sleep(1);

	if(msgrcv(clientQ, &cCreateGame, sizeof(struct clientCreateGame) - sizeof(long), 5, IPC_NOWAIT) != -1)
	{
		printf("> REQUEST RECEIVED: create a new game\n");
		createGame(clientQ);
	}
	if(msgrcv(clientQ, &cListGames, sizeof(struct clientListGames) - sizeof(long), 6, IPC_NOWAIT) != -1)
	{
		printf("> REQUEST RECEIVED: list all games\n");
		listGames(clientQ);
	}
	if(msgrcv(clientQ, &cJoinGame, sizeof(struct clientJoinGame) - sizeof(long), 9, IPC_NOWAIT) != -1)
	{
		printf("> REQUEST RECEIVED: join an existing game\n");
		matchPlayer(clientQ, cJoinGame.gameID);
	}

}

void activeGameService(game activeGame)
{
	struct clientMove receivedMove;
		receivedMove.type = 17;
	struct clientMove responseMove;
		responseMove.type = 18;
	//check for players moves in active games
	if(msgrcv(activeGame.gameQ, &receivedMove, sizeof(struct clientMove) - sizeof(long), 17, IPC_NOWAIT) != -1)
	{
		printf("> Received a move: %c%d -> %c%d.\n", receivedMove.movement[0] + 97, receivedMove.movement[1] + 1, receivedMove.movement[2] + 97, receivedMove.movement[3] + 1);
		int i;
		for(i = 0; i < 4; i++)
			responseMove.movement[i] = receivedMove.movement[i];
		//send move confirmation to both players
		if(msgsnd(activeGame.host.clientQ, &responseMove, sizeof(struct clientMove) - sizeof(long), 0) != -1)
			printf("> Move confirmation sent to: %s.\n", activeGame.host.name);
		if(msgsnd(activeGame.secondPlayer.clientQ, &responseMove, sizeof(struct clientMove) - sizeof(long), 0) != -1)
			printf("> Move confirmation sent to: %s.\n", activeGame.secondPlayer.name);
	}
}

int main(int argc, char const *argv[])
{

	int globalQ = openGlobalQ();
	while(1)
	{
		sleep(1);
		//check for new clients
		registerPlayer(globalQ);
		int i;
		//serve all registered clients
		for(i = 0; i < MAX_PLAYERS; i++)
		{
			if(playerTable[i].clientQ > 0)
				waitForRequest(playerTable[i].clientQ);
		}
		//serve all clients that are in an active game
		for(i = 0; i < MAX_PLAYERS; i++)
		{
			if(gameTable[i].inProgress == 2) // if game is active, wait for requests
			{
				activeGameService(gameTable[i]);
			}

		} 
	}

	return 0;
}