#include "dixonQtest.h"
#include "dixonQtest.c"

int main(){

	/* add_dixon_val(0.167); add_dixon_val(0.177); add_dixon_val(0.181);
	add_dixon_val(0.181);
	add_dixon_val(0.182); add_dixon_val(0.183); add_dixon_val(0.184);
	add_dixon_val(0.186); add_dixon_val(0.187); add_dixon_val(0.189);
	*/
	
	//1, 3, 5, 7, 8, 9, 13, 25
	add_dixon_val(1); add_dixon_val(3); add_dixon_val(5); add_dixon_val(7);
	add_dixon_val(8); add_dixon_val(9); add_dixon_val(13); add_dixon_val(25);

		//sort[0] = 8; sort[1]=10; sort[2]=13; sort[3]=15; sort[4]=17; 
		//sort[5]=19; sort[6]=21; sort[7]=36;

		//0.142, 0.153, 0.135, 0.002, 0.175 //0.002 is an outlier
		//sort[0] = 17; sort[1]=35; sort[2]=40; sort[3]=43; sort[4]=45; //17 is outlier
		//sort[0] = 31; sort[1]=35; sort[2]=40; sort[3]=43; sort[4]=67; //67 is outler
		
/*
//0.5980  0.5993  0.5995  0.5997  0.601  0.6400 0.6400 is an outlier for q95
 add_dixon_val(0.5980);  add_dixon_val(0.5993);  add_dixon_val(0.5995);
 add_dixon_val(0.5997);  add_dixon_val(0.601);  add_dixon_val(0.6400);
 */
}

