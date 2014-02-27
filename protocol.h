#ifndef PROTOCOL_H
#define PROTOCOL_H


/// Shared Memory fields
#define WHITE 0
#define BLACK 1
#define SKILL_BASIC 1
#define SKILL_MED 2
#define SKILL_ADV 3

/// Messages statuses
#define MSG_OK 0 //OK
#define MSG_FAILED 1 //Error
#define MSG_FULL 2 //Server is full

///CHESS BOARD
#define EMPTY 0
#define KING_WHITE 1
#define QUEEN_WHITE 2
#define ROOK_WHITE 3
#define BISHOP_WHITE 4
#define KNIGHT_WHITE 5
#define PAWN_WHITE 6

#define KING_BLACK 11
#define QUEEN_BLACK 12
#define ROOK_BLACK 13
#define BISHOP_BLACK 14
#define KNIGHT_BLACK 15
#define PAWN_BLACK 16

/*
PROTOCOL FOR A PROJECT FOR OPERATING SYSTEMS COURSE -- one for every student in a group, so that different clients and servers are able to communicate

Messages types listed (sorted by functions; sending via client queue if not stated differently):


Type    Short description
1       message clientLogin to log in to a server; sent via global queue
2       message serverConfirmLoginResponse to send a confirmation to a new client that wants to be registered

5       message clientCreateGame to send a request for creating a new game
14      message serverCreateGameResponse is a server's response to the above

6       message clientListGames to send a request for listing existing games
7       message serverListGamesResponse returns the number of existing games; filtering should be on client's side
8       message serverGameInfo is to be send as many times as the number returned by the above; each message contains basic game info

9       message clientJoinGame to send a request to join a certain game
10      message serverJoinGameResponse is server's response to the above; contains info about a given game: host's name, host's color, ID of queue to send messages about a move; server is to receive a move from player's queue
11      message serverNotifyClient is to start a game between two players; it is to be send to both players

12      message clientObserveGame is a request to join a game as a spectator
13      message serverObserveGameResponse is server's response to the above

16      message serverSyncBoard synchronizes a board between two players via server

17      message clientMove is a client's move request
18      message which is a server's response to the above, sent to both players in order to synchronize boards

// logging in
-- sending a message clientLogin via global queue
// logging out
-- closing a global queue on client's side

type = 1;
*/
struct clientLogin {
    long type;
    char name[40];
    int msgQueue; // used as key in msgget
    int shmAddr; // used as key in shmget
};

/*
/// DEMANDS A RESPONSE ///
Server sends a response with a status:
MSG_OK - loggin in OK / successfully created a new game
MSG_FAIL - error, i.e. wrong address in shmget
MSG_FULL - server/queue full

type = 2;
*/
struct serverConfirmLoginResponse {
    long type;
    int status;
};

/*
Struct that is to be read from shared memory.
Skill: number from 1 to 3
Color - 0 means white, 1 means black; white pieces always start a game
*/

struct clientShMemory {
    int skill;
    int color;
};

/*
Struct of requesting a new game by client.
type = 5;
*/
struct clientCreateGame {
    long type;
    int dummy;
};

/*
Server's response to client's request to create a new game.
type = 14;
*/
struct serverCreateGameResponse {
    long type;
    int status;
    int gameQueue;  // id used in msgget as 1st argument (key)
};

/*
Listing games - client's request
type = 6;
*/
struct clientListGames {
    long type;
    int dummy;
};

/*
Server's response to the above
type = 7;
*/
struct serverListGamesResponse {
    long type;
    int games;
};
/*
Sending info about one game; should be sent as many times as the number returned by the above
type = 8;
*/
struct serverGameInfo {
    long type;
    int gameID; 
    int skill; //host's level
    int color; //host's pieces' color
    char players[2][40]; //players' names; if there is only a host, second player's name is an empty string: ""
};

/*
Request to join an existing game
Needs a response: serverJoinGameResponse
type = 9;
*/
struct clientJoinGame {
    long type;
    int gameID;
};

/*
Server's response to the above
type = 10;
*/
struct serverJoinGameResponse {
    long type;
    int status; //MSG_OK, MSG_FAILED, MSG_FULL
    char name[40]; //host's name
    int color; //host's color
    int gameQueue;  // used in msgget as a 1st argument (key)
};

/*
Message that means the start of a game -- it is to be send to both players
type = 11;
*/
struct serverNotifyClient {
    long type;
    char name[40]; //opponent's name
};

/*
Request to observe a certain game
type = 12;
*/
struct clientObserveGame {
    long type;
    int gameID; 
};

/*
Server's response to the above
*/
struct serverObserveGameResponse {
    long type;
    int status;
    char name[2][40]; //players' names
    int color; //host's color

};
/*
Board's synchronization. After every move, server sends the whole board to spectators

type = 16;
*/
struct serverSyncBoard {
    long type;
    int board[8][8]; 
};

/*
Client's move.
Client should automatically update his board, as server should send this message to both players (and spectators).
After making a move, client should wait for this message (opponent's turn to make a move).
type = 17 when receiving from players
type = 18 when sending to players
*/
struct clientMove {
    long type;
    int movement[4]; //Four Integers in range 0..7
    /*
    i.e. for a board:
      A B
    1 k
    2

    message qith movement A1A2 will show a new board:
      A B
    1
    2 k

int[4] is: old column, old row, new column, new row. 
    */
};

//END OF GAME
/*
Server and clients should detect the end of the game individually.
The game ends when one of the kings is attacked.

Player can surrender by attacking his own king.
*/
#endif // PROTOCOL_H
