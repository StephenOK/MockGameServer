This program was tested on GCP instances using their internal IP addresses.

NOTE: Some elements of the code have been lifted from the Sample Socket Code from the COSC 439 files page. These elements were copied:
	-include statements for each file
	-argument checker
	-port input
	-sockaddr_in definition statements
	-socket declarations
	-UDP send and receive formats
	-TCP send and receive formats
	-address check
	-fork format
	-mmap format

Source for reading own IP Address:
https://www.tutorialspoint.com/how-to-get-the-ip-address-of-local-computer-using-c-cplusplus

All programs are compiled as:
	gcc -o <program name> <file name> -lm

To run server:
	<program name> <port number>

To initiate player:
	<program name> <server IP> <own port> <server port>

Commands for player:
	-login
	If player is not currently logged in to the game server, it will log them in. Player is logged in when program is initially run.

	-who
	Sends a query to the server asking for a list of the IDs of currently logged in users. The return includes the querying player, which is marked in the response.

	-play <playerID>
	Sends a query to the server for the indicated player's IP address and port number. Must input a number between 1-10 and not your own ID number. Once this data is acquired, a query is sent to the indicated player at which point the opponent must either accept or decline the request to start a game.


	-accept
	Can only be read after receiving a play request from another player. Confirms the initiation of a game with the querying player.

	-decline
	Can only be read after receiving a play request from another player. Declines the initiation of a game with the querying player.

	-move <column number>
	Can only be read after initiating a game with another player. Places the appropriate piece in the column of an active game. If the input is not between 1-7 or if the indicated column is full, notifies the player of the failed action. Once a move is successfully made, a message with the move made is sent to the opponent.

	-logout
	Logs the player out of the server. The program is still running, but it cannot receive messages from other players until a new login request is made.


	-exit
	Can only be read after logging out of the server. Closes the program fully.

