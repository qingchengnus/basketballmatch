#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

/*
CONSTANTS
*/
#define NUM_OF_ROUNDS 900
#define NUM_OF_PLAYERS 5
#define FIELD_PROCCESS 0
#define COURT_LENGTH 128
#define COURT_WIDTH 64
#define MIN_RUNNING_DISTANCE 1
#define MAX_RUNNING_DISTANCE 10
#define INITIAL_BALL_POSITION_X 64
#define INITIAL_BALL_POSITION_Y 32
#define BASKET_1_POSITION_X 0
#define BASKET_1_POSITION_Y 32
#define BASKET_2_POSITION_X 128
#define BASKET_2_POSITION_Y 32

/*
POINT STRUCTURE TO STORE POSITION
*/
typedef struct{
	int x;
	int y;
} point;

/*
PLAYER STRUCTURE TO STORE ATTRIBUTES OF A PLAYER
*/
typedef struct{
	point initial_position;
	point final_position;
	int distance_run;
	int reach_ball_count;
	int pass_ball_count;	
} player;

/*
FIELD STRUCTURE TO STORE ATTRIBUTES OF A FIELD
*/
typedef struct{
	point current_ball_position;
} field;

/*
HELP FUNCTION TO COMPUTE THE DISTANCE BETWEEN TWO POINTS
*/
int get_distance(point x, point y){
	return abs(x.x - y.x) + abs(x.y - y.y);
}

long long wall_clock_time()
{
#ifdef __linux__
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (long long)(tp.tv_nsec + (long long)tp.tv_sec * 1000000000ll);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)(tv.tv_usec * 1000 + (long long)tv.tv_sec * 1000000000ll);
#endif
}

/*
MAIN FUNCTION
*/
int main(int argc, char *argv[]){
	int rank, num_of_tasks;
	int current_round;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &num_of_tasks);
	
	srand(time(NULL) + rank);
	field current_field;
	player current_player;
	int recv_buffer[64];
	int send_buffer[64]; 
	long long before, after;

	
	if (num_of_tasks != NUM_OF_PLAYERS + 1){//incorrect number of proccess, exit
		//printf("Incorrect number of proccesses. Correct number of proccesses: %d .\n", NUM_OF_PLAYERS + 1);
		MPI_Finalize();
		return 0;
	} else {//correct number of proccesses
		before = wall_clock_time();
		for(current_round = 0; current_round < NUM_OF_ROUNDS; current_round ++){
			if (current_round == 0)//do initializations in the first round
			{
				if(rank == FIELD_PROCCESS){//initialize the position of the ball
					current_field.current_ball_position.x = INITIAL_BALL_POSITION_X;
					current_field.current_ball_position.y = INITIAL_BALL_POSITION_Y;
				} else {//initialize the position and other attributes of the player
					current_player.final_position.x = rand() % (COURT_LENGTH + 1);
					current_player.final_position.y = rand() % (COURT_WIDTH + 1);
					current_player.distance_run = 0;
					current_player.reach_ball_count = 0;
					current_player.pass_ball_count = 0;
				}
			}

			if (rank == FIELD_PROCCESS)//For field proccess
			{
				send_buffer[0] = current_field.current_ball_position.x;
				send_buffer[1] = current_field.current_ball_position.y;
				//Broadcast the position of the ball
				MPI_Bcast(&send_buffer, 2, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);

				//Gather the information: which players reached the ball
				MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
				//a copy of the receive buffer to save the reachability of players
				int status_player[NUM_OF_PLAYERS];
				//array to store the players who can reach the ball
				int reached_player[NUM_OF_PLAYERS];
				//a number to store how many plays that can reach the ball
				int num_of_reached_players = 0;
				int i;

				for(i = 1; i <= NUM_OF_PLAYERS; i++){
					status_player[i] = recv_buffer[i];
					if(recv_buffer[i] == 1){//this means the player can reach
						reached_player[num_of_reached_players] = i;
						num_of_reached_players ++;
					}
				}

				int win_player;
				if(num_of_reached_players == 0){//no player reachable
					win_player = -1;
				} else if(num_of_reached_players == 1){//only one player reachable
					win_player = reached_player[0];
				} else {//randomly choose a player to win the ball
					int win_player_index = rand() % num_of_reached_players;
					win_player = reached_player[win_player_index];
				}

				send_buffer[0] = win_player;

				
				
				//Scatter the winner of the ball
				if (win_player == -1)//no winner
				{
					for(i = 0; i < NUM_OF_PLAYERS + 1; i ++){
						send_buffer[i] = -1;
						
					}
				} else {
					for(i = 0; i < NUM_OF_PLAYERS + 1; i ++){
						
						send_buffer[i] = win_player;
						
						
					}
				}
				MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);

				//Gather the imformation: new ball position
				if(win_player != -1){
					MPI_Scatter(&send_buffer, 2, MPI_INT, &recv_buffer, 2, MPI_INT, win_player, MPI_COMM_WORLD);
					//printf("I have known the new position of  the ball\n");
					current_field.current_ball_position.x = recv_buffer[0];
					current_field.current_ball_position.y = recv_buffer[1];
				}
				
				//gather the information of each player
				MPI_Gather(&send_buffer, 7, MPI_INT, &recv_buffer, 7, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
				

				//printf("%d\n", current_round);
				//printf("%d %d\n", current_field.current_ball_position.x, current_field.current_ball_position.y);
					int did_reach_ball;
					int did_win_ball;
				for(i = 1; i <= NUM_OF_PLAYERS; i ++){
					did_reach_ball = 0;
					did_win_ball = 0;
					if(status_player[i] == 1){
						did_reach_ball = 1;
					}
					if(win_player == i + 1){
						did_win_ball = 1;
					}
					//printf("%d %d %d %d %d %d %d %d %d %d\n", i - 1, recv_buffer[i * 7], recv_buffer[i * 7 + 1], recv_buffer[i * 7 + 2], recv_buffer[i * 7 + 3], did_reach_ball, did_win_ball, recv_buffer[i * 7 + 4], recv_buffer[i * 7 + 5], recv_buffer[i * 7 + 6]);
				}


			} else {//this is a player
				//compute the distance to run in this round
				int current_running_distance = rand() % (MAX_RUNNING_DISTANCE - MIN_RUNNING_DISTANCE + 1) + MIN_RUNNING_DISTANCE;
				//the final position from last round is the initial position in this round
				current_player.initial_position = current_player.final_position;
				//get the position of the ball
				MPI_Bcast(&recv_buffer, 2, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
				point current_ball_position;
				current_ball_position.x = recv_buffer[0];
				current_ball_position.y = recv_buffer[1];
				//compute the distance between the player and the ball
				int distance_to_ball = get_distance(current_ball_position, current_player.initial_position);
				
				if(distance_to_ball <= current_running_distance){//the player can reach the ball
					//run to the position and update relevant attributes
					current_player.final_position = current_ball_position;
					current_player.distance_run += distance_to_ball;
					current_player.reach_ball_count ++;
					send_buffer[0] = 1;
					//Tell the field I have reached the ball
					MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
					//Hear from the field whether I win the ball
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
					if (recv_buffer[0] == rank)//Yeah, I win the ball!
					{
						
						current_player.pass_ball_count ++;
						//throw the ball to a random place
						send_buffer[0] = rand() % (COURT_LENGTH + 1);
						send_buffer[1] = rand() % (COURT_WIDTH + 1);
						int i;
						for(i = 1; i <= NUM_OF_PLAYERS; i ++){
							send_buffer[i * 2] = 0;
							send_buffer[i * 2 + 1] = 0;
						}
						//telling field the new position of the ball
						MPI_Scatter(&send_buffer, 2, MPI_INT, &recv_buffer, 2, MPI_INT, rank, MPI_COMM_WORLD);

					} else {
						//winner is telling field the new position of the ball, so I need to hear, but I'am not going to get the position
						MPI_Scatter(&send_buffer, 2, MPI_INT, &recv_buffer, 2, MPI_INT, recv_buffer[0], MPI_COMM_WORLD);
					}
				} else {//I cannot reach the ball
					//I just wander around
					
					int move_in_x = rand() % (current_running_distance + 1);
					int x_plus = rand() % 2;
					int move_in_y = current_running_distance - move_in_x;
					int y_plus = rand() % 2;
					if(x_plus == 1){
						if (current_player.initial_position.x + move_in_x <= COURT_LENGTH)
						{
							current_player.final_position.x = current_player.initial_position.x + move_in_x;
						} else {
							current_player.final_position.x = current_player.initial_position.x - move_in_x;
						}
					} else {
						if (current_player.initial_position.x - move_in_x >= 0)
						{
							current_player.final_position.x = current_player.initial_position.x - move_in_x;
						} else {
							current_player.final_position.x = current_player.initial_position.x + move_in_x;
						}
					}

					if(y_plus == 1){
						if (current_player.initial_position.y + move_in_y <= COURT_WIDTH)
						{
							current_player.final_position.y = current_player.initial_position.y + move_in_y;
						} else {
							current_player.final_position.y = current_player.initial_position.y - move_in_y;
						}
					} else {
						if (current_player.initial_position.y - move_in_y >= 0)
						{
							current_player.final_position.y = current_player.initial_position.y - move_in_y;
						} else {
							current_player.final_position.y = current_player.initial_position.y + move_in_y;
						}
					}
					
					current_player.distance_run += current_running_distance;
					send_buffer[0] = 0;
					//Tell the field I have not reached the ball
					MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);

					//Hear from field whether I win the ball, which is surely not.
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);

					if(recv_buffer[0] != -1){
						//winner is telling field the new position of the ball
						MPI_Scatter(&send_buffer, 2, MPI_INT, &recv_buffer, 2, MPI_INT, recv_buffer[0], MPI_COMM_WORLD);
					}
					
				}


				//telling field your attributes
				send_buffer[0] = current_player.initial_position.x;
				send_buffer[1] = current_player.initial_position.y;
				send_buffer[2] = current_player.final_position.x;
				send_buffer[3] = current_player.final_position.y;
				send_buffer[4] = current_player.distance_run;
				send_buffer[5] = current_player.reach_ball_count;
				send_buffer[6] = current_player.pass_ball_count;
				MPI_Gather(&send_buffer, 7, MPI_INT, &recv_buffer, 7, MPI_INT, FIELD_PROCCESS, MPI_COMM_WORLD);
			}
		}
	}
	MPI_Finalize();
	after = wall_clock_time();
	float time = ((float)(after - before))/1000000000;
	
	printf("Time Used: %1.6f\n",time);
 
	return 0;
}
