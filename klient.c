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
#include "protocol.h"

//draw a chess board according to board[][]
void drawBoard(int **board)
{
	int i, j;
	printf(" \ta b c d e f g h\n");
	for(i = 0; i < 8; i++)
	{
		printf("%d\t", i + 1);
		for(j = 0; j < 8; j++)
		{
			if (board[j][i] == KING_WHITE)
                printf("\u2654 ");
            else if (board[j][i] == QUEEN_WHITE)
                printf("\u2655 ");
            else if (board[j][i] == ROOK_WHITE)
                printf("\u2656 ");
            else if (board[j][i] == BISHOP_WHITE)
                printf("\u2657 ");
            else if (board[j][i] == KNIGHT_WHITE)
                printf("\u2658 ");
            else if (board[j][i] == PAWN_WHITE)
                printf("\u2659 ");
            else if (board[j][i] == KING_BLACK)
                printf("\u265A ");
            else if (board[j][i] == QUEEN_BLACK)
                printf("\u265B ");
            else if (board[j][i] == ROOK_BLACK)
                printf("\u265C ");
            else if (board[j][i] == BISHOP_BLACK)
                printf("\u265D ");
            else if (board[j][i] == KNIGHT_BLACK)
                printf("\u265E ");
            else if (board[j][i] == PAWN_BLACK)
                printf("\u265F ");
            else if (board[j][i] == EMPTY) 
                printf("- ");
        }
        printf("\n");
    }
}

//creates a new chess board with pieces
int** freshBoard()
{
	int **board;
	int i, j;
	board = (int**)malloc(8 * sizeof(int*));
	for(i = 0; i < 8; i++)
		board[i] = (int*)malloc(sizeof(int));
	
	//white and black pawns
	for(i = 0; i < 8; i++)
	{
		board[i][1] = PAWN_BLACK;
		board[i][6] = PAWN_WHITE;
	}
	//other pieces
	//row 0 - white pieces
	board[0][0] = ROOK_BLACK;
	board[1][0] = KNIGHT_BLACK;
	board[2][0] = BISHOP_BLACK;
	board[3][0] = QUEEN_BLACK;
	board[4][0] = KING_BLACK;
	board[5][0] = BISHOP_BLACK;
	board[6][0] = KNIGHT_BLACK;
	board[7][0] = ROOK_BLACK;	
	//row 7 - black pieces
	board[0][7] = ROOK_WHITE;
	board[1][7] = KNIGHT_WHITE;
	board[2][7] = BISHOP_WHITE;
	board[3][7] = QUEEN_WHITE;
	board[4][7] = KING_WHITE;
	board[5][7] = BISHOP_WHITE;
	board[6][7] = KNIGHT_WHITE;
	board[7][7] = ROOK_WHITE;
	//rows 1-6: empty
	for(j = 0; j < 8; j++)
		for(i = 2; i < 6; i++)
			board[j][i] = EMPTY;
	return board;
}

int connectToServer()
{
	int globalQueueID;
	printf("> Global queue ID: \n");
	scanf("%d", &globalQueueID);
	int created = msgget(globalQueueID, IPC_CREAT | 0666);
	if(created == -1)
	{
		perror("Global queue error!\n");
		return -1;
	}
	else 
		return created;
}

struct clientShMemory* setPreferences()
{
	struct clientShMemory *cShMemory;
	int cSharedMemId = shmget(getpid(), sizeof(struct clientShMemory), IPC_CREAT | 0666);
	cShMemory = (struct clientShMemory*)shmat(cSharedMemId, NULL, 0);

	do
	{
		printf("> Colour? 0 - white, 1 - black: ");
		scanf("%d", &(cShMemory->color));
	} while(cShMemory->color < 0 || cShMemory->color > 1);
	do
	{
		printf("> Level? 1-2-3: ");
		scanf("%d", &(cShMemory->skill));
	} while(cShMemory->skill < 1 || cShMemory->skill > 3);
	return cShMemory;
}

//zwraca uchwyt na kolejke klienta
int openpid(int queueID)
{
	int created = msgget(queueID, IPC_CREAT | 0666);
	if(created == -1)
	{
		perror("Client queue opening error!");
		return -1;
	}
	else
		return created;
}

//zwraca uchwyt na kolejke klienta
int loginPlayer(int globalQ, struct clientLogin *cLog, struct serverConfirmLoginResponse *conf)
{

	int pid = openpid(getpid());
	printf("Client queue opened with ID = %d\n", (*cLog).msgQueue);
	//wyslij globalna chec zalogowania sie
	if(msgsnd(globalQ, &(*cLog), sizeof(struct clientLogin) - sizeof(long), 0) != -1)
	{
		printf("> Waiting for server response...\n");
		//teraz odbierz z lokalnej potwierdzenie
		if(msgrcv(pid, &(*conf), sizeof(struct serverConfirmLoginResponse) - sizeof(long), 2, 0) != -1)
		{
			printf("> Server responded: \n");
			if((*conf).status == MSG_OK)
				printf("\t> Connection established.\n");
			else
			{
				printf("\t> Cannot establish connection with server.\n");
				return -1;
			}
		}
	}
	return pid;
}

//request of creating a new game
int requestNewGame(int localQ)
{
	struct clientCreateGame cCreateGame;
	cCreateGame.type = 5;

	struct serverCreateGameResponse sResponse;
		sResponse.type = 14;

	if(msgsnd(localQ, &cCreateGame, sizeof(struct clientCreateGame) - sizeof(long), 0) != -1)
	{
		printf("> Request sent.\n");
		if(msgrcv(localQ, &sResponse, sizeof(struct serverCreateGameResponse) - sizeof(long), 14, 0) != -1)
		{
			printf("> Server responded:\n");
			if(sResponse.status == MSG_OK)
			{
				printf("\t> New game created!\n");
				printf("\t> Your game queue ID: %d.\n", sResponse.gameQueue);
				int gameQ;

				//otworz kolejke gry
				if((gameQ = msgget(sResponse.gameQueue, IPC_CREAT | 0666)) != -1)
				{
					printf("\t> Game queue's handler ID: %d\n", gameQ);
					printf("> Game queue opened.\n");
					return gameQ;
				}
				else
					printf("> Cannot open game queue!\n");
			}
			else
				printf("\t> Cannot create new game!\n");
		}
	}
	return -1;
}

//list all existing games 
void listGames(int localQ, struct clientShMemory cShMemory)
{
	struct clientListGames cListGames;
		cListGames.type = 6;

	struct serverListGamesResponse sListGamesResponse;
		sListGamesResponse.type = 7;

	struct serverGameInfo sGameInfo;
		sGameInfo.type = 8;

	//wysylanie zapytanie o qylistowanie gier
	if(msgsnd(localQ, &cListGames, sizeof(struct clientListGames) - sizeof(long), 0) != -1)
	{
		printf("> List games request sent.\n");
		if(msgrcv(localQ, &sListGamesResponse, sizeof(struct serverListGamesResponse) - sizeof(long), 7, 0) != -1)
		{
			printf("> Server responded. \n\t> Number of existing games: %d\n", sListGamesResponse.games);
			if(sListGamesResponse.games > 0)
				printf("\t> Below listed only those that match your preferences.\n");
			int i = 0;
			//receive as many games as server declared
			for(i = 0; i < sListGamesResponse.games; i++)
			{
				if(msgrcv(localQ, &sGameInfo, sizeof(struct serverGameInfo) - sizeof(long), 8, 0) != -1)
				{
					if(sGameInfo.skill == cShMemory.skill && sGameInfo.color != cShMemory.color)
						printf(">>> Game %d\n\t>>> level: %d\n\t>>> host's colour: %d\n\t>>> host: %s\n\t>>> second player: %s\n", sGameInfo.gameID, sGameInfo.skill, sGameInfo.color, sGameInfo.players[0], sGameInfo.players[1]);
				}
				else
					printf("> Cannot list game number %d!\n", i);
			}
			printf("----------\n");
		}
		else
			printf("> Server did not respond!\n");
	}
	else
		printf("Cannot send a request!\n");
}

//Game play: sending and receiving moves of chess pieces
void playGame(int clientQ, int gameQ, struct clientShMemory cShMemory)
{
	struct serverNotifyClient sNotifyClient;
		sNotifyClient.type = 11;
	struct clientMove myMove;
		myMove.type = 17; // msg type for sending moves
	struct clientMove moveConfirmation;
		moveConfirmation.type = 18; // msg type for receiving confirmation - client queue
	int **board; // board pointer
	int color = cShMemory.color;
	int row0, row1;
	char column0, column1;
	//active waiting for a second player
	if(msgrcv(clientQ, &sNotifyClient, sizeof(struct serverNotifyClient) - sizeof(long), 11, 0) != -1)
	{
		printf("> Second player joined: %s.\n\t>> Now you can start the game!\n", sNotifyClient.name);
		board = freshBoard();
		while(1)
		{
			if(color == WHITE) // white pieces will start
			{
				drawBoard(board);
				do // ask for movement
				{
					printf(">>>\t Your move (column: a-h, row: 1-8): \n"); 
						printf("\t\t> Old column: ");
						getchar();
						scanf("%c", &column0);
						printf("\t\t> Old row: ");
						getchar();
						scanf("%d", &row0);
						printf("\t\t> New column: ");
						getchar();
						scanf("%c", &column1);
						printf("\t\t> New row: ");
						getchar();
						scanf("%d", &row1);
				}
				while(column0 < 97 || column0 > 104 || column1 < 97 || column1 > 104 || row0 < 1 || row0 > 8 || row1 < 1 || row1 > 8);
				myMove.movement[0] = column0 - 97;
				myMove.movement[1] = row0 - 1;
				myMove.movement[2] = column1 - 97;
				myMove.movement[3] = row1 - 1;
				if(msgsnd(gameQ, &myMove, sizeof(struct clientMove) - sizeof(long), 0) != -1)
				{
					printf("> Move sent: %c%d -> %c%d\n", myMove.movement[0] + 97, myMove.movement[1] + 1, myMove.movement[2] + 97, myMove.movement[3] + 1);
					board[myMove.movement[2]][myMove.movement[3]] = board[myMove.movement[0]][myMove.movement[1]];
					board[myMove.movement[0]][myMove.movement[1]] = EMPTY;

					if(msgrcv(clientQ, &moveConfirmation, sizeof(struct clientMove) - sizeof(long), 18, 0) != -1)
					{
						//move confirmation received
						printf("> Move confirmation received.\n");
						//check if it was surrender
						if(myMove.movement[0] + 97 == 'e' && myMove.movement[1] + 1 == 8 && myMove.movement[2] + 97 == 'e' && myMove.movement[3] + 1 == 8)
						{
							printf("\nYOU LOST by surrender!\n");
							exit(0);
						}
						else if(myMove.movement[2] + 97 == 'e' && myMove.movement[3] + 1 == 1)
						{
							printf("\nYOU WON!\n");
							exit(0);
						}
						drawBoard(board);
						//ative waiting for second player to make a move
						if(msgrcv(clientQ, &moveConfirmation, sizeof(struct clientMove) - sizeof(long), 18, 0) != -1)
						{
							printf("> Opponent move received: %c%d ->  %c%d\n", moveConfirmation.movement[0] + 97, moveConfirmation.movement[1] + 1, moveConfirmation.movement[2] + 97, moveConfirmation.movement[3] + 1);
							board[moveConfirmation.movement[2]][moveConfirmation.movement[3]] = board[moveConfirmation.movement[0]][moveConfirmation.movement[1] ];
							board[moveConfirmation.movement[0]][moveConfirmation.movement[1]] = EMPTY;
							//check if opponent won
							if(moveConfirmation.movement[2] + 97 == 'e' && moveConfirmation.movement[3] + 1 == 8)
							{
								printf("\n PRZEGRANA przez zbicie krola!\n");
								exit(0);
							}
						}
						else
							printf("> Cannot receive opponent's move confirmation!\n");
					}
					else
						printf("> Cannot reveive move confirmation!\n");

				}
				else
					printf("> Cannot send a move to server!\n");
			}
			else if(color == BLACK) //black pieces: active waiting for opponent to make a move
			{
				//receive opponent's move confirmation
				if(msgrcv(clientQ, &moveConfirmation, sizeof(struct clientMove) - sizeof(long), 18, 0) != -1)
				{
					printf("> Opponent's move received: %c%d ->  %c%d\n", moveConfirmation.movement[0] + 97, moveConfirmation.movement[1] + 1, moveConfirmation.movement[2] + 97, moveConfirmation.movement[3] + 1);
					board[moveConfirmation.movement[2]][moveConfirmation.movement[3]] = board[moveConfirmation.movement[0]][moveConfirmation.movement[1]];
					board[moveConfirmation.movement[0]][moveConfirmation.movement[1]] = EMPTY;
					if(moveConfirmation.movement[2] + 97 == 'e' && moveConfirmation.movement[3] + 1 == 1)
					{
						printf("\n YOU LOST!\n");
						exit(0);
					}
					drawBoard(board);
					do // ask for movement
					{
						printf(">>>\t Your move (column: a-h, row: 1-8): \n"); 
							printf("> Old column: ");
							getchar();
							scanf("%c", &column0);
							printf("> Old row: ");
							getchar();
							scanf("%d", &row0);
							printf("> New column: ");
							getchar();
							scanf("%c", &column1);
							printf("> New row: ");
							scanf("%d", &row1);
					}
					while(column0 < 97 || column0 > 104 || column1 < 97 || column1 > 104 || row0 < 1 || row0 > 8 || row1 < 1 || row1 > 8);
					myMove.movement[0] = column0 - 97;
					myMove.movement[1] = row0 - 1;
					myMove.movement[2] = column1 - 97;
					myMove.movement[3] = row1 - 1;
					//send move to server
					if(msgsnd(gameQ, &myMove, sizeof(struct clientMove) - sizeof(long), 0) != -1)
					{
						printf("> Movement sent: %c%d -> %c%d\n", myMove.movement[0] + 97, myMove.movement[1] + 1, myMove.movement[2] + 97, myMove.movement[3] + 1);
						board[myMove.movement[2]][myMove.movement[3]] = board[myMove.movement[0]][myMove.movement[1]];
						board[myMove.movement[0]][myMove.movement[1]] = EMPTY;
						if(msgrcv(clientQ, &moveConfirmation, sizeof(struct clientMove) - sizeof(long), 18, 0) != -1)
						{
							printf("> Movement confirmation received.\n");
							if(myMove.movement[0] + 97 == 'e' && myMove.movement[1] + 1 == 1 && myMove.movement[2] + 97 == 'e' && myMove.movement[3] + 1 == 1)
							{
								printf("\nYOU LOST by surrender!\n");
								exit(0);
							}
							else if(myMove.movement[2] + 97 == 'e' && myMove.movement[3] + 1 == 8)
							{
								printf("\nYOU WON!\n");
								exit(0);
							}
							drawBoard(board);
						}
						else
							printf("> Cannot receive move confirmation!\n");
					}
					else
						printf("> Cannot send move to server!\n");
				}
				else
					printf("> Cannot receive opponent's move confirmation!\n");
			}
		}
	}
	else
		printf("> Failed to wait for an opponent!");
}

//Returns a handler to a game queue
int joinGame(int clientQ)
{
	int option;
	struct clientJoinGame cJoinGame;
		cJoinGame.type = 9;
	struct serverJoinGameResponse sJoinGameResponse;
		sJoinGameResponse.type = 10;
	printf(">>> ID of a game you want to join: ");
	scanf("%d", &option);
	cJoinGame.gameID = option;
	if(msgsnd(clientQ, &cJoinGame, sizeof(struct clientJoinGame) - sizeof(long), 0) != -1)
	{
		printf("> Request sent, you want to join a game with ID = %d\n", cJoinGame.gameID);
		if(msgrcv(clientQ, &sJoinGameResponse, sizeof(struct serverJoinGameResponse) - sizeof(long), 10, 0) != -1)
		{
			printf("> Server responded.\n");
			if(sJoinGameResponse.status == MSG_OK)
			{
				printf("\t>> You successfully joined a game with ID = %d\n", cJoinGame.gameID);
				printf("\t>> Your opponent: %s\n", sJoinGameResponse.name);
				int gameQ;
				if((gameQ = msgget(cJoinGame.gameID, IPC_CREAT | 0666)) != -1)
				{
					printf("> Game queue opened successfully.\n");
					return gameQ;
				}
				else
					printf("> Cannot open game queue!\n");
			}
			else
			{
				printf("\n>> Cannot join a game with such ID!\n");
			}
		}
	}
	else
		printf("> Cannot send a request!\n");
	return -1;

}

int main(int argc, char const *argv[])
{
	//server queue ID
	int globalQ;
	int pid = getpid();
	//client queue ID
	int localQ;
	//game queue ID
	int gameQ;
	int option;
	struct clientLogin cLogin;
		cLogin.type = 1;
		cLogin.msgQueue = pid;
		cLogin.shmAddr = pid;

	struct serverConfirmLoginResponse sConfirmLoginResponse;
		sConfirmLoginResponse.type = 2;

	printf("> Your name: ");
	scanf("%s", &(*cLogin.name));

	struct clientShMemory *cShMemory;
		cShMemory = setPreferences();

	if((globalQ = connectToServer()) == -1)
		perror("Cannot connect with server!");
	
	if((localQ = loginPlayer(globalQ, &cLogin, &sConfirmLoginResponse)) == -1)
		perror("Cannot log in!");
	while(1)
	{
		do
		{
			printf("\n-----\n> What do you want to do?\n");
			printf("\t>\t1 -- New game\n");
			printf("\t>\t2 -- Join existing game\n");
			printf("\t>\t3 -- list existing games\n");
			printf("\t>\t4 -- Exit\n");
			printf("\t>\t5 -- Draw a new board\n");
			scanf("%d", &option);
		}
		while(option != 1 && option != 2 && option != 3 && option != 4 && option != 5);

		// new game - create and play
		if(option == 1)
		{
			if((gameQ = requestNewGame(localQ)) == -1)
				perror("RequestNewGame error!");
			else
				playGame(localQ, gameQ, *cShMemory);
		}

		//join existing game and play
		if(option == 2)
		{
			if((gameQ = joinGame(localQ)) != -1)
				playGame(localQ, gameQ, *cShMemory);
			else
				perror("Cannot join a game");
		}

		//list existing games
		if(option == 3)
			listGames(localQ, *cShMemory);

		// exit
		if(option == 4)
			exit(0);

		//draw a new board
		if(option == 5)
		{
			printf("Plansza:\n\n");
			drawBoard(freshBoard());
		}
	}
	
	return 0;
}