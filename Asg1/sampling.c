#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

//data structure for representing an individual sinusoid of the form amp*sin(2*pi/tp + phi)
typedef struct _signal{
	double amp;
	int tp;
	int phi;
}signal;

#define PI 3.14159
#define NUM 2
#define QL 100

signal arr[5];  //array storing component sinusoids of source signal

double destValuesX[10000],destValuesY[10000];  //arrays storing x and y coordinates of noisy sampled points respectively

//function to get the plotting command for gnuplot
char * getPlotCommand(signal arr[], int STP)
{
	char * commandsForGNUPlot = (char *)malloc(1000*sizeof(char));

	char str[100];
	char ampStr[10];
	char tpStr[10];
	char phiStr[10];

	int i;
	
	char STPstr[10];
	snprintf(STPstr, sizeof(STPstr), "%d", (2*STP));
	strcpy(commandsForGNUPlot,"set title 'Signals'\nset xlabel 'Time(t)-->'\nset ylabel 'y-->'\nset xrange [0:");  //setting labels for axis
	strcat(commandsForGNUPlot, STPstr);
	strcat(commandsForGNUPlot, "]\nplot ");

	char fn[500];   //source signal
	strcpy(fn,"");

	for(i=0;i<4;i++)
	{
		snprintf(ampStr,sizeof(ampStr),"%.2lf",arr[i].amp);
		snprintf(tpStr,sizeof(tpStr),"%d",arr[i].tp);
		snprintf(phiStr,sizeof(phiStr),"%d",arr[i].phi);
		
		strcpy(str,ampStr);
		strcat(str,"*sin(2*pi*x/");
		strcat(str,tpStr);
		strcat(str," + ");
		strcat(str,phiStr);
		strcat(str,") + ");

		strcat(fn,str);
	}

	snprintf(ampStr,sizeof(ampStr),"%.2lf",arr[4].amp);
	snprintf(tpStr,sizeof(tpStr),"%d",arr[4].tp);
	snprintf(phiStr,sizeof(phiStr),"%d",arr[4].phi);
		
	strcpy(str,ampStr);
	strcat(str,"*sin(2*pi*x/");
	strcat(str,tpStr);
	strcat(str," + ");
	strcat(str,phiStr);
	strcat(str,")");
	
	strcat(fn,str);
	
	printf("\nSource signal is : %s\n\n",fn);  //source signal is printed
	
	strcat(commandsForGNUPlot,fn);
	
	strcat(commandsForGNUPlot," linetype 1 title 'Source Signal', 'data.temp' with lines ls 3 title 'Destination Signal'\n");

	return commandsForGNUPlot;
}

//returns the value of source signal for a particular time
double getSourceValue(signal arr[],double t)
{
	int i;
	double val = 0;

	for(i=0;i<5;i++)
	{
		val+=(arr[i].amp)*sin((2*PI*t/arr[i].tp) + arr[i].phi);
	}
	
	return val;
}

//returns the minimum time period among the component signals for source signal
int minTimePeriod(signal arr[])
{
	int min = arr[0].tp;
	
	int i;
	
	for(i=1;i<5;i++)
		if(arr[i].tp<min)
			min = arr[i].tp;
			
	return min;
}

//returns the LCM of two numbers
int LCM2(int a, int b)
{
	int p1 = a,p2=b;
	
	while (b > 0) 
	{
		a = a % b;
		a ^= b; b ^= a; a ^= b; 
	} 
	
	return (p1*p2)/a;
}

//returns the time period for the source signal
int sourceTP(signal arr[])
{
	int i;
	int LCM = arr[0].tp;
	
	for(i=1;i<5;i++)
	{
		LCM = LCM2(LCM,arr[i].tp);
	}
		
	return LCM;
}

//returns quantized value for given y-value and quantization level
double getQuantizedValue(double y,int L)
{
	double levelWidth = (double)(20.0/(double)L);
	
	int nearLevel = (int)((y+10.0)/levelWidth);
	
	if((y+10)-nearLevel*levelWidth>=((nearLevel+1)*levelWidth-(y+10)))  //if q.l. is the upper one
		return (nearLevel+1)*levelWidth-10.0;
	else  //if q.l. is the lower one
		return nearLevel*levelWidth-10.0;
}

//generates randomized source signal in 'arr'
void generateSourceSignal()
{
	int i;
	
	for(i=0;i<5;i++)
	{
		arr[i].amp = ((double)rand()/(double)RAND_MAX) * 2;
		arr[i].tp = rand()%3+1;
		arr[i].phi = rand()%10;
	}
}

//performs sampling and quantization for the various sampling points, and returns the number of sample points
int quantization(double sampleFreq,int L,int STP)
{
	//Quantizing the source signal
	int k = 0;
	double xsample;
	
	for(xsample = 0.0;xsample<=2*STP;xsample+=1/sampleFreq)
	{
		destValuesX[k] = xsample;
		destValuesY[k] = getQuantizedValue(getSourceValue(arr, xsample), L);  //quantized value for value at xsample			
		k++;
	}
	
	return k;
}

//Adds random noise to the signal
void noiseAddition(int k)
{
	int i;
	
	for(i=0;i<k;i++)
	{
		destValuesY[k] += pow(-1, rand()%2) * ((double)rand()/(double)RAND_MAX)*0.05; //random value generated and added		
	}
}

//re-constucts waveform from the noisy signal using spline-interpolation and returns the rms error
double reconstruction(int k)
{
	gsl_interp_accel *acc = gsl_interp_accel_alloc ();
        gsl_spline *spline = gsl_spline_alloc (gsl_interp_cspline, k);
        
    	gsl_spline_init (spline, destValuesX, destValuesY, k); 	
    	
    	FILE * temp = fopen("data.temp", "w");  //for storing points to be plotted
    	
    	double ERROR_RMS = 0.0;
    
    	double xi, yi; 
    	
    	//spline interpolation
    	for (xi = destValuesX[0]; xi <= destValuesX[k-1]; xi += 0.01)
    	{
        	yi = gsl_spline_eval (spline, xi, acc);
        	
        	ERROR_RMS += (yi - getSourceValue(arr, xi)) * (yi - getSourceValue(arr, xi));  //squares....(1)
		
	        fprintf (temp, "%lf %lf\n", xi, yi);
        }
        
        gsl_spline_free (spline);
        gsl_interp_accel_free (acc);
  	
  	fclose(temp); //temp freed
  	
  	int samples = (int)((destValuesX[k-1] - destValuesX[0])/0.01);
  	ERROR_RMS/=samples;  //mean....(2)
  	
  	ERROR_RMS = sqrt(ERROR_RMS);  //root....(3)
  	
  	//from (1), (2) & (3), we get RMS of error
  	
  	return ERROR_RMS;
}

//plots the points using gnuplot
void plotGraph(char* command)
{
	FILE * gnuplotPipe = popen("gnuplot -persistent","w");
	
  	fprintf(gnuplotPipe, "%s \n", command); //Send commands to gnuplot one by one.
  	pclose(gnuplotPipe);
}

//part 1 of the assignment
void part1()
{
	int minTP,STP,k;
	double f,sampleFreq;
	
	generateSourceSignal();  //source signal generated
	
	STP = sourceTP(arr);  //source time period determined
	minTP = minTimePeriod(arr);  //minimum time period determined
	f = (double)(1.0/(double)minTP);  //max frequency derived from minimum time period
	
	char * command = getPlotCommand(arr, STP);  //command for gnu plot obtained
	
	int L;
	
	//Chosen sampling frequency and levels of Quantization
	sampleFreq = NUM*f;
	L = QL;
	
	k = quantization(sampleFreq,L,STP);  //sampling and quantization performed
	
	printf("Sampling performed with sample frequency : %f\n\n",sampleFreq);
	printf("Division of permissible range performed with quantization level : %d\n\n",L);
	
	noiseAddition(k);  //random noise added
	
	printf("Random noise added\n\n");
	
	double ERROR_RMS = reconstruction(k);  //reconstruction performed	
  	plotGraph(command);
  	
  	printf("Re-construction of waveform from noisy signal performed\n\n");  	
  	
  	printf("The Root mean sqaure error is %lf\n\n",ERROR_RMS);  //RMS displayed
}

//part 2 of assignment
void part2()
{
	int L = QL, k;
	double sampleFreq;
	int STP = sourceTP(arr);
	
	double prev_ERROR,curr_ERROR,max_DIFF, fMax;
	
	FILE * fp = fopen("Error.temp", "w");
	
	k = quantization(0.6,L,STP);
	noiseAddition(k);
	prev_ERROR = reconstruction(k);
	max_DIFF = 0.0;	
	
	//loop for recording when minimum error is encountered
	for(sampleFreq = 0.7;sampleFreq<=5.0;sampleFreq+=0.1)
	{		
		k = quantization(sampleFreq,L,STP);
		
		noiseAddition(k);	
		curr_ERROR = reconstruction(k);
		fprintf(fp, "%lf %lf\n", sampleFreq, curr_ERROR);
		
		if(prev_ERROR - curr_ERROR > max_DIFF)
		{
			fMax = sampleFreq/2;  //max freq is half of the sampling rate
			max_DIFF = prev_ERROR - curr_ERROR;
		}
		
		prev_ERROR = curr_ERROR;
	}
	
	fclose(fp);
	
	char * command = "set title 'RMS_ERROR vs Sampling Frequency'\nplot 'Error.temp' with lines ls 1";
	plotGraph(command);
	
	printf("The predicted value of fMax is : %lf\n\n",fMax);
}

//main function
int main()
{
	srand((unsigned int)time(NULL));  //seeding randomize function with time
	
	part1();
  	
  	part2();
  	
  	return 0;	
}