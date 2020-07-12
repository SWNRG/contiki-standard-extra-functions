/* Author: George Violettas, georgevio@uom.edu.gr
 * 2020-02-01
 
 * Implementing a Dixon Q-test Function inspired from
 * Python implementation: https://sebastianraschka.com/Articles/2014_dixon_test.html
 
    Keyword arguments:
        data = A ordered or unordered list of data points (int or float).
        left = Q-test of minimum value in the ordered list if True.
        right = Q-test of maximum value in the ordered list if True.
        q_dict = A dictionary of Q-values for a given confidence level,
            where the dict. keys are sample sizes N, and the associated values
            are the corresponding critical Q values. E.g.,
            {3: 0.97, 4: 0.829, 5: 0.71, 6: 0.625, ...}

 *   In python, returns a list of 2 values for the outliers, or None.
 *   E.g.,
 *      for [1,1,1] -> [None, None]
 *      for [5,1,1] -> [None, 5]
 *      for [5,1,5] -> [1, None]
 *
 * Read about Dixon Q test here: 
 * https://www.statisticshowto.datasciencecentral.com/dixons-q-test/
 */

// use these in another program to test. 27 is an outlier
	//add_dixon_val(8); add_dixon_val(9); add_dixon_val(13); add_dixon_val(27);
	//add_dixon_val(1); add_dixon_val(3); add_dixon_val(5); add_dixon_val(7);

	/* printing floats
	float res ;
	int nom = 3; int den = 11;
	res = (nom / (float)den)*1000;
	printf("RESULT normal = %03u\n",(int)res);	
	printf("RESULT hack = %03d.%03u\n",(int)res, (int)(res*1000-(int)res));
   */
   
#include<stdio.h> 
//#include "dixonQtest.h"   

#ifndef PRINT_ON
#define PRINT_ON 1
#endif

#define PRINT_DETAILS 0 //prints all details if outlier found

/* minimum is 3 NOT zero, i.e., 0.941 is for 3 incoming values */    
static float Q90[] = 
		{0.941, 0.765, 0.642, 0.560, 0.507, 0.468, 0.437,
       0.412, 0.392, 0.376, 0.361, 0.349, 0.338, 0.329,
       0.320, 0.313, 0.306, 0.300, 0.295, 0.290, 0.285, 
       0.281, 0.277, 0.273, 0.269, 0.266, 0.263, 0.260
       };

static float Q95[] = 
		{0.970, 0.829, 0.710, 0.625, 0.568, 0.526, 0.493, 0.466,
       0.444, 0.426, 0.410, 0.396, 0.384, 0.374, 0.365, 0.356,
       0.349, 0.342, 0.337, 0.331, 0.326, 0.321, 0.317, 0.312,
       0.308, 0.305, 0.301, 0.290
      };

static float Q99[] = 
		{0.994, 0.926, 0.821, 0.740, 0.680, 0.634, 0.598, 0.568,
       0.542, 0.522, 0.503, 0.488, 0.475, 0.463, 0.452, 0.442,
       0.433, 0.425, 0.418, 0.411, 0.404, 0.399, 0.393, 0.388,
       0.384, 0.380, 0.376, 0.372
       };

static void quicksort_i(int number[],int first,int last);   
static uint8_t queue_index_i = 0; /* no queue in standard C? */
static int incoming_values_i[dixon_n_vals];
float critical_value_i = 0; //confidence level to compare with
static uint8_t all_values_exist_i = 0; /* array is not populated yet */
/**************************************************************/ 
static void sortInt_i(int grade[], int n){
	int i, j, swapped;
	int temp;
	for (i = 0; i < n; ++i){
		 for (j = i + 1; j < n; ++j)
		 {
		     if (grade[i] > grade[j])
		     {
		         temp = grade[i];
		         grade[i] = grade[j];
		         grade[j] = temp;
		     }//end if
		 }//end inner for
	}//end outer for
#if PRINT_ON
	/*
	for (i = 0; i < n; ++i){
		 printf("q_after: %d\n", grade[i]);
	}//end for
	*/
#endif
}//end sortInt_i
/**************************************************************/
/* return 0 if there is no outlier in the current data,
 * return 1 if the incoming value is a small outlier,
 * return 2 if the incoming value is a big outlier
 */
static int addDixonQIn(int value){
	uint8_t answer = 0;
	float q_small;
	float q_big;

	incoming_values_i[queue_index_i] = value;
	
	queue_index_i++;
#if PRINT_ON 
	printf("queue_index_i: %d\n", queue_index_i);
#endif
	if (queue_index_i == dixon_n_vals){
#if PRINT_ON
		printf("Reseting queue_index in dixonQtestIn = 0\n");
#endif
		queue_index_i = 0;	
		all_values_exist_i = 1; /* array has now all cells populated */
	}	
	
	if(all_values_exist_i == 1){ /* start calculating possible outliers */
		int sort[dixon_n_vals];
		uint8_t i;
		
		for( i=0;i<dixon_n_vals;i++){
			sort[i] = incoming_values_i[i];
		}
		
		sortInt_i(sort, dixon_n_vals); /* sort a NEW array */
		
		/* Denominator = (x[n]-x[1] */
		int denomInt = sort[dixon_n_vals-1] - sort[0];	
#if PRINT_ON 
		printf("Q_last=%d - Q_0=%d",sort[dixon_n_vals-1], sort[0]);
		printf(" --> denom= %d\n",denomInt);
#endif	
		if( denomInt != 0){ /* avoid division by zero */		
			/* Lower Outlier: Q = (x[2]-x[1])/(x[n]-x[1]) */		
			q_small = (float) (sort[1] - sort[0]) / (float) denomInt;
#if PRINT_ON			
			printf("Q_1=%d - Q_0=%d / denom=%03u",sort[1],sort[0],denomInt);							
			/* printing float */
			printf(" --> q_small: 0.%03u\n",(int)(q_small*1000-(int)q_small));							
#endif									
			/* Higher Outlier: Q = (x[n]-x[n-1])/(x[n]-x[1]) */
			q_big = (float) (sort[dixon_n_vals-1] - sort[dixon_n_vals -2]) / (float) denomInt;
#if PRINT_ON			
			printf("sort[last]=%d - sort[last-1]=%d / denom=%d",
					sort[dixon_n_vals-1], sort[dixon_n_vals-2], denomInt);			
			/* printing float */
			printf(" --> q_big: 0.%03u\n",(int)(q_big*1000-(int)q_big));					
#endif					 		 
		}
#if PRINT_ON
		else
			printf("Denominator == 0, cannot check outliers...\n");
#endif
		if (confidence_level == 0){
			critical_value_i = Q90[dixon_n_vals-3];			
		}
		else if (confidence_level == 1){
			critical_value_i = Q95[dixon_n_vals-3];		
		}
		else if (confidence_level == 2){
			critical_value_i = Q99[dixon_n_vals-3];				
		}
#if PRINT_ON	
		printf("Critical value (qxx) to compare: ");
		/* printing float */
		printf("0.%03u\n",(int)(critical_value_i*1000-(int)critical_value_i));
#endif		
		/* first confidence for n==3 */
		if(q_small > critical_value_i){ 
#if PRINT_DETAILS
			printf("Outlier=%d, ", sort[0]);
			printf(" with q_small=");
			printf("0.%03u",(int)(q_small*1000-(int)q_small));
			printf(" > 0.%03u",(int)(critical_value_i*1000-(int)critical_value_i));
			printf(", in [");
			int i=0;		
			for(i=0;i<dixon_n_vals;i++){
				printf("%d ",sort[i]);
			}
			printf("]\n");
#endif
			answer = 1;
		}
		if(q_big > critical_value_i){
#if PRINT_DETAILS
			printf("Outlier=%d, ",sort[dixon_n_vals-1]);
			printf(" with q_big=");
			printf("0.%03u",(int)(q_big*1000-(int)q_big));
			printf(" > 0.%03u",(int)(critical_value_i*1000-(int)critical_value_i));
			int i=0;
			printf(" in [");
			for(i=0;i<dixon_n_vals;i++){
				printf("%d ",sort[i]);
			}
			printf("]\n");
#endif
			answer = 2;
		}

#if PRINT_ON		
		/* Print only for testing */
		if(q_small > Q95[dixon_n_vals-3]) 
			printf("q_small is an outlier in q95\n");
		if(q_small > Q99[dixon_n_vals-3]) 
			printf("q_small is an outlier in q99\n");			
#endif
	}
	return answer;
}

/* TODO: it freezes. never checked the error */
static void quicksort_i(int number[],int first,int last){
	int i, j, pivot, temp;
	if(first<last){
		pivot=first;
		i=first;
		j=last;
		while(i<j){
			while(number[i]<=number[pivot]&&i<last)
				i++;
			while(number[j]>number[pivot])
				j--;
			if(i<j){
				temp=number[i];
				number[i]=number[j];
				number[j]=temp;
			}
		}
		temp=number[pivot];
		number[pivot]=number[j];
		number[j]=temp;
		quicksort(number,first,j-1);
		quicksort(number,j+1,last);
	}
}


