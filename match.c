#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

/*
CONSTANTS
*/
#define NUM_OF_ROUNDS_PER_HALF 2700
#define NUM_OF_PLAYERS_PER_TEAM 5
#define FIELD_0_PROCCESS 10
#define FIELD_1_PROCCESS 11
#define COURT_LENGTH 128
#define COURT_WIDTH 64
#define INITIAL_BALL_POSITION_X 64
#define INITIAL_BALL_POSITION_Y 32
#define BASKET_1_POSITION_X 0
#define BASKET_1_POSITION_Y 32
#define BASKET_2_POSITION_X 128
#define BASKET_2_POSITION_Y 32
#define BALL_UNCERTAINTY_RANGE 8
#define TEAM_0_SPEED 8
#define TEAM_0_DRIBBLING 5
#define TEAM_0_SHOOTING 2
#define TEAM_0_SHOOT_MAX_DISTANCE 8

#define TEAM_1_SPEED 8
#define TEAM_1_DRIBBLING 6
#define TEAM_1_SHOOTING 1
#define TEAM_1_SHOOT_MAX_DISTANCE 40

/*
SOME HELPER STRUCTURES TO STORE ATTRIBUTES
*/
typedef struct{
	int x;
	int y;
} point;

typedef struct{
	point initial_position;
	point final_position;
	int team;
	int speed;
	int dribbling;
	int shooting;	
} player;

typedef struct{
	int has_ball;
	point current_ball_position;
	int defending_team;
	int team0_score;
	int team1_score;
	point players_position[NUM_OF_PLAYERS_PER_TEAM * 2];
} field;

/*
SOME HELPER FUNCTIONS
*/
int get_distance(point x, point y){
	return abs(x.x - y.x) + abs(x.y - y.y);
}

int min(int x, int y){
	if(x < y){
		return x;
	} else {
		return y;
	}
}

double doublemin(double x, double y){
	if(x < y){
		return x;
	} else {
		return y;
	}
}

int max(int x, int y){
	if(x < y){
		return y;
	} else {
		return x;
	}
}

double doublesqrt(double num)
{
	if(num >= 0)
	{
		double x = num;
		int i;
		for(i = 0; i < 20; i ++)
		{
		x = (((x * x) + num) / (2 * x));
		}
		return x;
	} else {
		return -1;
	}
}

int get_field_number(point x){
	if(x.x <= (COURT_LENGTH / 2)){
		return FIELD_0_PROCCESS;
	} else {
		return FIELD_1_PROCCESS;
	}
}

int is_inside_field(point x){
	if (x.x <= COURT_LENGTH && x.x >= 0 && x.y <= COURT_WIDTH && x.y >= 0){
		return 1;
	} else {
		return 0;
	}
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
	int current_half = 0;
	long long before, after;

	before = wall_clock_time();
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &num_of_tasks);
	int tag = 1;
	MPI_Status status;
	field current_field;
	player current_player;
	int recv_buffer[128];
	int send_buffer[128];
	srand(time(NULL) + rank);
	int is_new_game = 1;
	
	if (num_of_tasks != NUM_OF_PLAYERS_PER_TEAM * 2 + 2){//incorrect number of proccesses given
		MPI_Finalize();
		return 0;
	} else {
		//initialize some variable
		current_field.team0_score = 0;
		current_field.team1_score = 0;
		current_half = 0;
		for(current_round = 0; current_round < NUM_OF_ROUNDS_PER_HALF * 2; current_round ++){
			if (is_new_game == 1 || current_round == NUM_OF_ROUNDS_PER_HALF)//new game: ball position back to mid point, each team back to their defending half of the field
			{
				is_new_game = 0;
				if(current_round == NUM_OF_ROUNDS_PER_HALF){//from now on is the second half of the game
					current_half = 1;
				}

				//initialize field 0
				if(rank == FIELD_0_PROCCESS){
					current_field.has_ball = 1;
					current_field.current_ball_position.x = INITIAL_BALL_POSITION_X;
					current_field.current_ball_position.y = INITIAL_BALL_POSITION_Y;
					if(current_half == 0){
						current_field.defending_team = 0;
						int i = 0;
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM; i ++){
							MPI_Recv(&recv_buffer, 2, MPI_INT, i, 0, MPI_COMM_WORLD,&status);
							current_field.players_position[i].x = recv_buffer[0];
							current_field.players_position[i].y = recv_buffer[1];
						}

						for(i = NUM_OF_PLAYERS_PER_TEAM; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}
					} else {
						current_field.defending_team = 1;
						int i = 0;
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM; i ++){
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}

						for(i = NUM_OF_PLAYERS_PER_TEAM; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
							MPI_Recv(&recv_buffer, 2, MPI_INT, i, current_round, MPI_COMM_WORLD,&status);
							current_field.players_position[i].x = recv_buffer[0];
							current_field.players_position[i].y = recv_buffer[1];
						}
					}
					
				} else if(rank == FIELD_1_PROCCESS){//initialize field 1
					current_field.has_ball = 0;
					current_field.current_ball_position.x = INITIAL_BALL_POSITION_X;
					current_field.current_ball_position.y = INITIAL_BALL_POSITION_Y;
					if(current_half == 0){
						current_field.defending_team = 1;
						int i = 0;
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM; i ++){
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}

						for(i = NUM_OF_PLAYERS_PER_TEAM; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
							MPI_Recv(&recv_buffer, 2, MPI_INT, i, current_round, MPI_COMM_WORLD, &status);
							current_field.players_position[i].x = recv_buffer[0];
							current_field.players_position[i].y = recv_buffer[1];
						}
					} else {
						current_field.defending_team = 0;
						int i = 0;
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM; i ++){
							MPI_Recv(&recv_buffer, 2, MPI_INT, i, 0, MPI_COMM_WORLD,&status);
							current_field.players_position[i].x = recv_buffer[0];
							current_field.players_position[i].y = recv_buffer[1];
						}

						for(i = NUM_OF_PLAYERS_PER_TEAM; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}
					}

				} else {//initialize players
					if(current_half == 0){
						if(rank < NUM_OF_PLAYERS_PER_TEAM){
							current_player.final_position.x = rand() % (COURT_LENGTH/2 + 1);
							current_player.final_position.y = rand() % (COURT_WIDTH + 1);
							current_player.team = 0;
							current_player.speed = TEAM_0_SPEED;
							current_player.dribbling = TEAM_0_DRIBBLING;
							current_player.shooting = TEAM_0_SHOOTING;
							send_buffer[0] = current_player.final_position.x;
							send_buffer[1] = current_player.final_position.y;
							MPI_Send(&send_buffer, 2, MPI_INT, FIELD_0_PROCCESS, 0, MPI_COMM_WORLD);

						} else {
							current_player.final_position.x = rand() % (COURT_LENGTH/2) + COURT_LENGTH/2 + 1;
							current_player.final_position.y = rand() % (COURT_WIDTH + 1);
							current_player.team = 1;
							current_player.speed = TEAM_1_SPEED;
							current_player.dribbling = TEAM_1_DRIBBLING;
							current_player.shooting = TEAM_1_SHOOTING;
							send_buffer[0] = current_player.final_position.x;
							send_buffer[1] = current_player.final_position.y;
							MPI_Send(&send_buffer, 2, MPI_INT, FIELD_1_PROCCESS, current_round, MPI_COMM_WORLD);
						}
					} else {
						if(rank < NUM_OF_PLAYERS_PER_TEAM){
							
							current_player.final_position.x = rand() % (COURT_LENGTH/2) + COURT_LENGTH/2 + 1;
							current_player.final_position.y = rand() % (COURT_WIDTH + 1);
							current_player.team = 0;
							current_player.speed = TEAM_0_SPEED;
							current_player.dribbling = TEAM_0_DRIBBLING;
							current_player.shooting = TEAM_0_SHOOTING;
							send_buffer[0] = current_player.final_position.x;
							send_buffer[1] = current_player.final_position.y;
							MPI_Send(&send_buffer, 2, MPI_INT, FIELD_1_PROCCESS, 0, MPI_COMM_WORLD);

						} else {
							current_player.final_position.x = rand() % (COURT_LENGTH/2 + 1);
							current_player.final_position.y = rand() % (COURT_WIDTH + 1);
							current_player.team = 1;
							current_player.speed = TEAM_1_SPEED;
							current_player.dribbling = TEAM_1_DRIBBLING;
							current_player.shooting = TEAM_1_SHOOTING;
							send_buffer[0] = current_player.final_position.x;
							send_buffer[1] = current_player.final_position.y;
							MPI_Send(&send_buffer, 2, MPI_INT, FIELD_0_PROCCESS, current_round, MPI_COMM_WORLD);
						}
					}
					
					
				}
			}

			if(rank == FIELD_0_PROCCESS || rank == FIELD_1_PROCCESS){//it is a field
				send_buffer[0] = current_field.current_ball_position.x;
				send_buffer[1] = current_field.current_ball_position.y;
				int proccess_with_ball;
				//determine which proccess currently own the ball
				if((current_field.has_ball && rank == FIELD_0_PROCCESS) || (!current_field.has_ball && rank == FIELD_1_PROCCESS)){
					proccess_with_ball = FIELD_0_PROCCESS;
				} else {
					proccess_with_ball = FIELD_1_PROCCESS;
				}
				int i = 0;

				//tell the players on this field where the ball is
				for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
					if(current_field.players_position[i].x != -1){
						MPI_Send(&send_buffer, 2, MPI_INT, i, 2 * current_round, MPI_COMM_WORLD);
					}
				}
				int winner;
				if(current_field.has_ball == 1){//for field own the ball
					//gather the players which can reach the ball
					MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, rank, MPI_COMM_WORLD);
					int max = 0;
					int max_count = 0;
					int max_player_numer[NUM_OF_PLAYERS_PER_TEAM * 2];
					//determine the winner of the ball
					for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2; i++){
						if (recv_buffer[i] > max)
						{
							max = recv_buffer[i];
							max_count = 1;
							max_player_numer[max_count - 1] = i;

						} else if(recv_buffer[i] == max){
							max_count ++;
							max_player_numer[max_count - 1] = i;
						}	
					}
					if(max_count != 0){
						int winner_index = rand() % max_count;
						winner = max_player_numer[winner_index];
					} else {
						winner = -1;
					}

					//no winner!
					if (winner == -1)
					{
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2 + 2; i ++){
							send_buffer[i] = -1;
						}
					} else {//there is a winner
						for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2 + 2; i ++){
							send_buffer[i] = winner;
						}
					}
					//tell the players who the winner is
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, proccess_with_ball, MPI_COMM_WORLD);
				} else {//for the field doesn't own the ball
					//send and receive some dummy message to avoid deadlock
					if(rank == FIELD_0_PROCCESS){
			-			MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_1_PROCCESS, MPI_COMM_WORLD);
						MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_1_PROCCESS, MPI_COMM_WORLD);
					} else {
						MPI_Gather(&send_buffer, 3, MPI_INT, &recv_buffer, 3, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
						MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
					}
					//get the winner
					winner = recv_buffer[0];
				}
				if(winner != -1){//if there is a winner, field need to listen to the new position of the ball
					//listen to the new position of the ball
					MPI_Recv(&recv_buffer, 4, MPI_INT, winner, 3 * current_round, MPI_COMM_WORLD, &status);
					int target_x = recv_buffer[0];
					int target_y = recv_buffer[1];
					int winner_shooting_skill = recv_buffer[2];
					int ball_distance_from_target = recv_buffer[3];
					if(target_x != -1){//ok, the target position is on my field
						//determine whether the player shoot perfectly
						double shooting_threshold = doublemin(100.0, (10 + 90 * winner_shooting_skill)/(0.5 * ball_distance_from_target * doublesqrt(ball_distance_from_target * 1.0) - 0.5));
						double random_shoot = (rand() % 100001) / 100000.0;
						if(random_shoot < shooting_threshold){//got it
							//if this is a goal, how many points is gonna gain?
							int point_gained = 2;
							if(ball_distance_from_target > 24){
								point_gained = 3;
							}
							//ok, this is a goal, give the scores to the correct team
							if((target_x == BASKET_1_POSITION_X && target_y == BASKET_1_POSITION_Y) || (target_x == BASKET_2_POSITION_X && target_y == BASKET_2_POSITION_Y)){
								if(target_x == BASKET_1_POSITION_X && target_y == BASKET_1_POSITION_Y){
									if (rank == FIELD_0_PROCCESS)
									{
										if(current_field.defending_team == 0){
											current_field.team1_score += point_gained;
										} else {
											current_field.team0_score += point_gained;
										}
									} else {
										if(current_field.defending_team == 0){
											current_field.team0_score += point_gained;
										} else {
											current_field.team1_score += point_gained;
										}
									}
								} else {
									if (rank == FIELD_0_PROCCESS)
									{
										if(current_field.defending_team == 0){
											current_field.team0_score += point_gained;
										} else {
											current_field.team1_score += point_gained;
										}
									} else {
										if(current_field.defending_team == 0){
											current_field.team1_score += point_gained;
										} else {
											current_field.team0_score += point_gained;
										}
									}
								}
								
								//reinitialze the field and the players, everyone goes back to their defending half of the field
								is_new_game = 1;

							} else {//this is not a goal, just put the ball to the target location
								current_field.current_ball_position.x = target_x;
								current_field.current_ball_position.y = target_y;
								current_field.has_ball = 1;
								if(!is_inside_field(current_field.current_ball_position)){//the ball is outside the field
									is_new_game = 1;
								}
							}
						} else {//didn't get there!
							//to a random location within the 8 feets from the target
							int move_in_x = rand() % BALL_UNCERTAINTY_RANGE;
							int x_plus = rand() % 2;
							int move_in_y = rand() % BALL_UNCERTAINTY_RANGE;
							int y_plus = rand() % 2;
							if(x_plus == 1){
								current_field.current_ball_position.x = target_x + move_in_x;
							} else {
								current_field.current_ball_position.x = target_x - move_in_x;
							}

							if(y_plus == 1){
								current_field.current_ball_position.y = target_y + move_in_y;
							} else {
								current_field.current_ball_position.y = target_x - move_in_y;
							}

							if(get_field_number(current_field.current_ball_position) == rank){
								current_field.has_ball = 1;
							} else {
								current_field.has_ball = 0;
							}

							if(!is_inside_field(current_field.current_ball_position)){//it is outside the field!
								is_new_game = 1;

							}
						}
						//synchronize some possibly changed info the another field
						int target_proccess;
						if(rank == FIELD_0_PROCCESS){
							target_proccess = FIELD_1_PROCCESS; 
						} else {
							target_proccess = FIELD_0_PROCCESS;
						}
						send_buffer[0] = current_field.current_ball_position.x;
						send_buffer[1] = current_field.current_ball_position.y;
						send_buffer[2] = current_field.team0_score;
						send_buffer[3] = current_field.team1_score;
						send_buffer[4] = is_new_game;
						if(get_field_number(current_field.current_ball_position) == rank){
							current_field.has_ball = 1;
						} else {
							current_field.has_ball = 0;
						}
						MPI_Send(&send_buffer, 5, MPI_INT, target_proccess, 4 * current_round, MPI_COMM_WORLD);

					} else {//I don't own the ball
						//waiting for another field to give me the updated information
						int target_proccess;
						if(rank == FIELD_0_PROCCESS){
							target_proccess = FIELD_1_PROCCESS; 
						} else {
							target_proccess = FIELD_0_PROCCESS;
						}
						MPI_Recv(&recv_buffer, 5, MPI_INT, target_proccess, 4 * current_round, MPI_COMM_WORLD, &status);
						current_field.current_ball_position.x = recv_buffer[0];
						current_field.current_ball_position.y = recv_buffer[1];
						current_field.team0_score = recv_buffer[2];
						current_field.team1_score = recv_buffer[3];
						is_new_game = recv_buffer[4];
						if(get_field_number(current_field.current_ball_position) == rank){
							current_field.has_ball = 1;
						} else {
							current_field.has_ball = 0;
						}
					}
					
				}
				if(rank == FIELD_0_PROCCESS){//field 0 is in charge of the printing jobs
					//gather the information from its players
					MPI_Gather(&recv_buffer, 9, MPI_INT, &send_buffer, 9, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, FIELD_1_PROCCESS, MPI_COMM_WORLD);
					//gather the information of players in the other field
					MPI_Recv(&recv_buffer, 90, MPI_INT, FIELD_1_PROCCESS, 5 * current_round, MPI_COMM_WORLD, &status);
					int i = 0;

					//tell other proccess whether the next round will be a new game!
					int temp_send_buffer[NUM_OF_PLAYERS_PER_TEAM * 2 + 2];
					for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2 + 2; i ++){
						temp_send_buffer[i] = is_new_game;
					}
					MPI_Scatter(&temp_send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
					//printing job
					//printf("%d\n",current_round);
					//printf("%d %d\n",current_field.team0_score, current_field.team1_score);
					//printf("%d %d\n", current_field.current_ball_position.x, current_field.current_ball_position.y);
					

					//update its knowledge of the players on itself
					for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
						if(send_buffer[i * 9] != -1){
							//printf("%d %d %d %d %d %d %d %d %d %d\n", i, send_buffer[i * 9], send_buffer[i * 9 + 1], send_buffer[i * 9 + 2], send_buffer[i * 9 + 3], send_buffer[i * 9 + 4], send_buffer[i * 9 + 5], send_buffer[i * 9 + 6], send_buffer[i * 9 + 7], send_buffer[i * 9 + 8]);
							current_field.players_position[i].x = send_buffer[i * 9 + 2];
							current_field.players_position[i].y = send_buffer[i * 9 + 3];
						} else {
							//printf("%d %d %d %d %d %d %d %d %d %d\n", i, recv_buffer[i * 9], recv_buffer[i * 9 + 1], recv_buffer[i * 9 + 2], recv_buffer[i * 9 + 3], recv_buffer[i * 9 + 4], recv_buffer[i * 9 + 5], recv_buffer[i * 9 + 6], recv_buffer[i * 9 + 7], recv_buffer[i * 9 + 8]);
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}
					}
				} else {//this is field 1
					//gather the information from its players
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, FIELD_1_PROCCESS, MPI_COMM_WORLD);

					//send the information of its player to field 0
					MPI_Send(&recv_buffer, 90, MPI_INT, FIELD_0_PROCCESS, 5 * current_round, MPI_COMM_WORLD);

					//update its knowledge of the players on itself
					for(i = 0; i < NUM_OF_PLAYERS_PER_TEAM * 2; i ++){
						if(recv_buffer[i * 9] != -1){
							current_field.players_position[i].x = recv_buffer[i * 9 + 2];
							current_field.players_position[i].y = recv_buffer[i * 9 + 3];
						} else {
							current_field.players_position[i].x = -1;
							current_field.players_position[i].y = -1;
						}
					}

					//learn from field 0 whether this is gonna be a new game!
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
					is_new_game = recv_buffer[0];
				}
			} else {//this is a player
				//the initial position in this round the final position from last round
				current_player.initial_position.x = current_player.final_position.x;
				current_player.initial_position.y = current_player.final_position.y;

				//initialize of attributes
				int reached_ball = 0;
				int kicked_ball = 0;
				int ball_challenge = -1;
				point shoot_target;
				shoot_target.x = -1;
				shoot_target.y = -1;

				//get the ball position from its field
				MPI_Recv(&recv_buffer, 2, MPI_INT, get_field_number(current_player.initial_position), 2 * current_round, MPI_COMM_WORLD, &status);
				point ball_position;
				ball_position.x = recv_buffer[0];
				ball_position.y = recv_buffer[1];
				int target_field;
				int not_target_field;
				//set the target field to send his reachability to the ball
				target_field  = get_field_number(ball_position);
				if(target_field == FIELD_0_PROCCESS){
					not_target_field = FIELD_1_PROCCESS;
				} else {
					not_target_field = FIELD_0_PROCCESS;
				}

				int distance_to_ball = get_distance(current_player.initial_position, ball_position);
				if(distance_to_ball <= current_player.speed){//I can reach the ball
					reached_ball = 1;
					//move to the ball
					current_player.final_position = ball_position;

					//compute my challenge and send it to the target field
					ball_challenge = (rand() % 10 + 1) * current_player.dribbling;
					send_buffer[0] = ball_challenge;
					MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, target_field, MPI_COMM_WORLD);
					//get the result of whether I win the ball
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, target_field, MPI_COMM_WORLD);
					if (recv_buffer[0] == rank)//I win the ball!
					{
						kicked_ball = 1;

						//get the correct basket to shoot!
						point basket_position;
						int distance_to_basket;
						if (current_player.team == 0)
						{
							if(current_half == 0){
								basket_position.x = BASKET_2_POSITION_X;
								basket_position.y = BASKET_2_POSITION_Y;
							} else {
								basket_position.x = BASKET_1_POSITION_X;
								basket_position.y = BASKET_1_POSITION_Y;
							}

							distance_to_basket = get_distance(basket_position,ball_position);
							if(distance_to_basket > TEAM_0_SHOOT_MAX_DISTANCE){//it is too far way, I'll just throw the ball toward our attacking direction
								if(current_half == 0){
									basket_position.x = min(COURT_LENGTH, ball_position.x + TEAM_0_SHOOT_MAX_DISTANCE);
									basket_position.y = ball_position.y;
								} else {
									basket_position.x = max(0, ball_position.x - TEAM_0_SHOOT_MAX_DISTANCE);
									basket_position.y = ball_position.y;
								}
							}

							//tell the field where I want to shoot
							shoot_target = basket_position;
							send_buffer[0] = basket_position.x;
							send_buffer[1] = basket_position.y;
							send_buffer[2] = current_player.shooting;
							send_buffer[3] = get_distance(basket_position,ball_position);
							target_field = get_field_number(basket_position);
							if(target_field == FIELD_0_PROCCESS){
								not_target_field = FIELD_1_PROCCESS;
							} else {
								not_target_field = FIELD_0_PROCCESS;
							}
							MPI_Send(&send_buffer, 4, MPI_INT, target_field, 3 * current_round, MPI_COMM_WORLD);
							//give another field dummy value
							send_buffer[0] = -1;
							MPI_Send(&send_buffer, 4, MPI_INT, not_target_field, 3 * current_round, MPI_COMM_WORLD);
						} else {//the same for team 1
							if(current_half == 0){
								basket_position.x = BASKET_1_POSITION_X;
								basket_position.y = BASKET_1_POSITION_Y;
							} else {
								basket_position.x = BASKET_2_POSITION_X;
								basket_position.y = BASKET_2_POSITION_Y;
							}

							distance_to_basket = get_distance(basket_position,ball_position);
							if(distance_to_basket > TEAM_1_SHOOT_MAX_DISTANCE){
								if(current_half == 1){
									basket_position.x = min(COURT_LENGTH, ball_position.x + TEAM_1_SHOOT_MAX_DISTANCE);
									basket_position.y = ball_position.y;
								} else {
									basket_position.x = max(0, ball_position.x - TEAM_1_SHOOT_MAX_DISTANCE);
									basket_position.y = ball_position.y;
								}
							}
							shoot_target = basket_position;
							send_buffer[0] = basket_position.x;
							send_buffer[1] = basket_position.y;
							send_buffer[2] = current_player.shooting;
							target_field = get_field_number(shoot_target);
							if(target_field == FIELD_0_PROCCESS){
								not_target_field = FIELD_1_PROCCESS;
							} else {
								not_target_field = FIELD_0_PROCCESS;
							}
							send_buffer[3] = get_distance(basket_position,ball_position);
							MPI_Send(&send_buffer, 4, MPI_INT, target_field, 3 * current_round, MPI_COMM_WORLD);
							send_buffer[0] = -1;
							MPI_Send(&send_buffer, 4, MPI_INT, not_target_field, 3 * current_round, MPI_COMM_WORLD);
						}
					}
					
				} else {//I cannot reach the ball, I'll just move towards the ball as fast as possible
					if (current_player.initial_position.x > ball_position.x) {
						current_player.final_position.x = max(ball_position.x, current_player.initial_position.x - current_player.speed);
						int horizontal_movement = current_player.initial_position.x - current_player.final_position.x;
						int remaining_speed = current_player.speed - horizontal_movement;
						if(current_player.initial_position.y > ball_position.y){
							current_player.final_position.y = max(ball_position.y, current_player.initial_position.y - remaining_speed);	
						} else {
							current_player.final_position.y = min(ball_position.y, current_player.initial_position.y + remaining_speed);
						}
						
					} else {
						current_player.final_position.x = min(ball_position.x, current_player.initial_position.x + current_player.speed);
						int horizontal_movement = current_player.final_position.x - current_player.initial_position.x;
						int remaining_speed = current_player.speed - horizontal_movement;
						if(current_player.initial_position.y > ball_position.y){
							current_player.final_position.y = max(ball_position.y, current_player.initial_position.y - remaining_speed);	
						} else {
							current_player.final_position.y = min(ball_position.y, current_player.initial_position.y + remaining_speed);
						}
					}


					send_buffer[0] = -1;
					//tell the field I cannot reach the ball
					MPI_Gather(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, target_field, MPI_COMM_WORLD);
					//listen to the field for the winner, of course it won't be me		
					MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, target_field, MPI_COMM_WORLD);
				}

				if (get_field_number(current_player.final_position) == FIELD_0_PROCCESS)//finally I am in field 0
				{
					target_field = FIELD_0_PROCCESS;
					not_target_field = FIELD_1_PROCCESS;
					send_buffer[0] = current_player.initial_position.x;
					send_buffer[1] = current_player.initial_position.y;
					send_buffer[2] = current_player.final_position.x;
					send_buffer[3] = current_player.final_position.y;

					send_buffer[4] = reached_ball;
					send_buffer[5] = kicked_ball;
					send_buffer[6] = ball_challenge;

					send_buffer[7] = shoot_target.x;
					send_buffer[8] = shoot_target.y;

					//send my info to field 0
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, target_field, MPI_COMM_WORLD);

					//send dummy values to the other field
					send_buffer[0] = -1;
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, not_target_field, MPI_COMM_WORLD);
				} else {// I'm finally in field 1
					target_field = FIELD_1_PROCCESS;
					not_target_field = FIELD_0_PROCCESS;
					//send dummy values to the other field
					send_buffer[0] = -1;
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, not_target_field, MPI_COMM_WORLD);

					//send my info to field 1
					send_buffer[0] = current_player.initial_position.x;
					send_buffer[1] = current_player.initial_position.y;
					send_buffer[2] = current_player.final_position.x;
					send_buffer[3] = current_player.final_position.y;

					send_buffer[4] = reached_ball;
					send_buffer[5] = kicked_ball;
					send_buffer[6] = ball_challenge;

					send_buffer[7] = shoot_target.x;
					send_buffer[8] = shoot_target.y;
					MPI_Gather(&send_buffer, 9, MPI_INT, &recv_buffer, 9, MPI_INT, target_field, MPI_COMM_WORLD);
					
				}
				//get from field 0 whether the next round is gonna be a new game!
				MPI_Scatter(&send_buffer, 1, MPI_INT, &recv_buffer, 1, MPI_INT, FIELD_0_PROCCESS, MPI_COMM_WORLD);
				is_new_game = recv_buffer[0];

			}
		}
		MPI_Finalize();

		after = wall_clock_time();
		float time = ((float)(after - before))/1000000000;
		
		printf("Time Used: %1.6f\n",time);
    		return 0;
	}
	
}
