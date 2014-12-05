//Adding required libraries
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <string>

using namespace std;

//defining constants used in the implementation
#define PI 3.14159
#define sampleFreq 4096
#define numPoints 64
#define NoiseAmp 0.002
#define SourceAmp 2

int startFreq1 = 100000000;
int startFreq2 = 100006400;
int L = 100;

//data structure for representing an individual sinusoid of the form amp*sin(2*pi/tp + phi)
typedef struct _signal{
	double amp;
	int freq;
	int phi;
}signal;

signal u1[5],u2[5];  //array storing component sinusoids of source signals u1 and u2

struct cpx
{
  cpx(){}
  cpx(double aa):a(aa){}
  cpx(double aa, double bb):a(aa),b(bb){}
  double a;
  double b;
  double modsq(void) const
  {
    return a * a + b * b;
  }
  cpx bar(void) const
  {
    return cpx(a, -b);
  }
};

double mod(cpx a)
{
    return sqrt(a.a * a.a + a.b * a.b);
}

cpx operator +(cpx a, cpx b)
{
  return cpx(a.a + b.a, a.b + b.b);
}

cpx operator *(cpx a, cpx b)
{
  return cpx(a.a * b.a - a.b * b.b, a.a * b.b + a.b * b.a);
}

cpx operator /(cpx a, cpx b)
{
  cpx r = a * b.bar();
  return cpx(r.a / b.modsq(), r.b / b.modsq());
}

cpx operator /(cpx a, double b)
{
  return cpx(a.a / b, a.b / b);
}

cpx EXP(double theta)
{
  return cpx(cos(theta),sin(theta));
}

cpx time1source[1000], time1dest[1000], time2source[1000], time2dest[1000];
cpx timesent[10000000], freq1[10000000], freq2[1000];

const double two_pi = 4 * acos(0);

// in:     input array
// out:    output array
// step:   {SET TO 1} (used internally)
// size:   length of the input/output {MUST BE A POWER OF 2}
// dir:    either plus or minus one (direction of the FFT)
// RESULT: out[k] = \sum_{j=0}^{size - 1} in[j] * exp(dir * 2pi * i * j * k / size)
void FFT(cpx *in, cpx *out, int step, int size, int dir)
{
  if(size < 1) return;
  if(size == 1)
  {
    out[0] = in[0];
    return;
  }
  FFT(in, out, step * 2, size / 2, dir);
  FFT(in + step, out + size / 2, step * 2, size / 2, dir);
  for(int i = 0 ; i < size / 2 ; i++)
  {
    cpx even = out[i];
    cpx odd = out[i + size / 2];
    out[i] = even + EXP(dir * two_pi * i / size) * odd;
    out[i + size / 2] = even + EXP(dir * two_pi * (i + size / 2) / size) * odd;
  }
}

//generates randomized source signal in 'arr'
void generateSourceSignal(signal arr[])
{
	int i;
	
	for(i=0;i<5;i++)
	{
		arr[i].amp = ((double)rand()/(double)RAND_MAX) * SourceAmp;  //amplitude
		arr[i].freq = rand()%3480 + 20;  //frequency
		arr[i].phi = rand()%10;  //phase difference
	}
}

//calculates fMax, max frequency
int calcFMax()
{
	int freqmax = 0;
	for (int i = 0; i < 5; ++i)
	{
		if(u1[i].freq > freqmax)
			freqmax = u1[i].freq;
	}
	for (int i = 0; i < 5; ++i)
	{
		if(u2[i].freq > freqmax)
			freqmax = u2[i].freq;
	}
	return freqmax;
}

//calculates fMin, min frequency
int calcFMin()
{
	int freqmin = 10000;
	for (int i = 0; i < 5; ++i)
	{
		if(u1[i].freq < freqmin)
			freqmin = u1[i].freq;
	}
	for (int i = 0; i < 5; ++i)
	{
		if(u2[i].freq < freqmin)
			freqmin = u2[i].freq;
	}
	return freqmin;
}

//calculates Bandwidth
int calcBandwidth()
{
	return calcFMax() - calcFMin();
}

//returns the value of source signal for a particular time
double getSourceValue(signal arr[],double t)
{
	int i;
	double val = 0;

	for(i=0;i<5;i++)
	{
		val+=(arr[i].amp)*sin((2*PI*t*arr[i].freq) + arr[i].phi);
	}
	
	return val;
}

//returns quantized value for given y-value and quantization level
cpx getQuantizedValue(cpx p)
{
	cpx qv;
	double q;
	double m = mod(p);
	double levelWidth = (double)(0.02/(double)L);
	
	int nearLevel = (int)((m)/levelWidth);
	
	if((m)-(double)(nearLevel*levelWidth)>=((double)((nearLevel+1)*levelWidth)-(m)))  //if q.l. is the upper one
	{
		q = (double)((nearLevel+1)*levelWidth);
		qv.a = (p.a)*q/m;
		qv.b = (p.b)*q/m;
	}
	else  //if q.l. is the lower one
	{
		q = (double)((nearLevel)*levelWidth);
		qv.a = (p.a)*q/m;
		qv.b = (p.b)*q/m;
	}
	
	return qv;
}

//performs quantization for the various sampling points, and returns the number of sample points
void Quantization(cpx arr[],int n)
{	
	for(int i=0;i<n;i++)
	{
		arr[i] = getQuantizedValue(arr[i]);  //quantized value for value			
	}
}

//function to plot grapg using gnu-plot
void plotGraph(char * title, cpx arr[], int n, int step)
{
	//plots the points using gnuplot
	char command[1000];
	strcpy(command, "set title '");
	strcat(command, title);
	strcat(command, "'\nplot 'data.temp' with lines ls 3");

	FILE * fp = fopen("data.temp", "w");
	double sample = 0.0;
	for(int i = 0;i<n;i+=step)
	{
		//printf("%lf + j%lf\n", arr[i].a, arr[i].b);
		fprintf(fp, "%lf %lf\n", sample, sqrt(pow(arr[i].a, 2) + pow(arr[i].b, 2)));
		sample+=(double)(step)/sampleFreq;
	}
	fclose(fp);

	FILE * gnuplotPipe = popen("gnuplot -persistent","w");
	
  	fprintf(gnuplotPipe, "%s \n", command); //Send commands to gnuplot one by one.
  	pclose(gnuplotPipe);

}

//Adds random noise to the signal
void noiseAddition(cpx arr[], int n)
{
	for(int i=0;i<n;i++)
	{
		arr[i].a += pow(-1, rand()%2) * ((double)rand()/(double)RAND_MAX)*NoiseAmp; //random value generated and added		
	}
}

//performs sampling of signal
void sampling()
{
	double sample = 0.0;
	for(int i=0;i<numPoints;++i)
	{
		time1source[i] = cpx(getSourceValue(u1, sample), 0);
		time2source[i] = cpx(getSourceValue(u2, sample), 0);

		sample+=1.0/sampleFreq;
	}
}

//Function to shift signals to higher frequency range
//Returns the total number of points in shifted array
//The shifted array is stored in arr1
int shiftFreqHigher(cpx arr1[], cpx arr2[])
{
	int step = sampleFreq / numPoints;
	int startIndex1 = startFreq1 / step;  //index of start frequency of first signal in array
	int startIndex2 = startFreq2 / step;  //index of start frequency of second signal in array

	cpx middle = cpx(arr1[0].a/(numPoints/2+1),0);

	//Initializing entries to 0 for overlap conditions
	for(int i = 0;i<numPoints;++i)
	{
		arr1[i+startIndex1] = cpx(0,0);
		arr1[i+startIndex2] = cpx(0,0);
	}

	for(int i = 0;i<numPoints;++i)
	{
		arr1[i+startIndex1] = arr1[i+startIndex1] + arr1[i];
		arr1[i+startIndex2] = arr1[i+startIndex2] + arr2[i];
	}

	//Padding zeros before
	for(int i=0;i<startIndex1;++i)
	{
		arr1[i] = cpx(0,0);
	}

	for (int i = startIndex1+numPoints; i < startIndex2; ++i)
	{
		arr1[i] = cpx(0,0);
	}

	int power2 = (int)pow(2, ceil(log(startIndex2+numPoints)/log(2.0)));
	//Padding zeros after
	for(int i = startIndex2+numPoints;i<power2;++i)
	{
		arr1[i] = cpx(0,0);
	}

	arr1[power2] = middle;

	//Reflecting the signal
	for (int i = power2+1; i < 2*power2; ++i)
	{
		arr1[i] = arr1[2*power2 - i].bar();
	}

	return 2*power2;  //returning power
}

//Function to shift signals to lower frequency range
void shiftFreqLower(cpx arr1[], cpx arr2[])
{
	int step = sampleFreq / numPoints;
	int startIndex1 = startFreq1 / step;
	int startIndex2 = startFreq2 / step;

	for(int i = 0;i<numPoints;++i)
	{
		if(i+startIndex1 > startIndex2)//overlap
		{
			arr1[i] = arr1[i+startIndex1] / 2.0;
			arr2[i+startIndex1 - startIndex2] = arr1[i+startIndex1] / 2.0;
		}
		else
			arr1[i] = arr1[i+startIndex1];
	}

	int i;
	if (startIndex1 + numPoints < startIndex2)//No overlap
		i = 1;
	else
		i = startIndex1 + numPoints - startIndex2;


	for(;i<numPoints;++i)
	{
		arr2[i] = arr1[i+startIndex2];
	}
}

//Operations performed on source signals
int processingAtSource()
{
	//signal generation
	generateSourceSignal(u1);
	generateSourceSignal(u2);

	//sampling
	sampling();	

	//graph plotting
	plotGraph((char *)("Source 1"), time1source, numPoints, 1);
	plotGraph((char *)("Source 2"), time2source, numPoints, 1);

	//FFT
	FFT(time1source, freq1, 1, numPoints, 1);
	FFT(time2source, freq2, 1, numPoints, 1);

	//Shift to higher frequency range
	int shiftedPoints = shiftFreqHigher(freq1, freq2);

	//IFFT
	FFT(freq1, timesent, 1, shiftedPoints, -1);

	//Dividing by number of points after IFFT
	for (int i = 0; i < shiftedPoints; ++i)
	{
		timesent[i].a  /= (double)shiftedPoints; 
		timesent[i].b  /= (double)shiftedPoints;
	}

	return shiftedPoints;
}

//Operations performed on destination signals
void processingAtDest(int shiftedPoints)
{
	//FFT
	FFT(timesent, freq1, 1, shiftedPoints, 1);

	//Shift to lower frequency range
	shiftFreqLower(freq1, freq2);

	//IFFT
	FFT(freq1, time1dest, 1, numPoints, -1);
	FFT(freq2, time2dest, 1, numPoints, -1);

	//Dividing by number of points after IFFT
	for (int i = 0; i < numPoints; ++i)
	{
		time1dest[i].a  /= numPoints;
		time2dest[i].a  /= numPoints;  
	}

	//Graph plotting
	plotGraph((char *)("Destination 1"), time1dest, numPoints, 1);
	plotGraph((char *)("Destination 2"), time2dest, numPoints, 1);

	//Computing ERROR_RMS
	double ERROR_RMS1 = 0.0, ERROR_RMS2 = 0.0;
	for (int i = 0; i < numPoints; ++i)
	{
		ERROR_RMS1 += (mod(time1source[i])-mod(time1dest[i]))*(mod(time1source[i])-mod(time1dest[i]));
		ERROR_RMS2 += (mod(time2source[i])-mod(time2dest[i]))*(mod(time2source[i])-mod(time2dest[i]));
	}

	ERROR_RMS1 /= numPoints;
	ERROR_RMS2 /= numPoints;

	ERROR_RMS1 = sqrt(ERROR_RMS1);
	ERROR_RMS2 = sqrt(ERROR_RMS2);

	printf("The error for u1 is %lf and for u2 is %lf\n", ERROR_RMS1, ERROR_RMS2);
}

//returns the theoretical optimal frequency
int theoreticalOptimalFrequency()
{
	return sampleFreq/2;
}

//computes experimental optimal frequency and compares with theoretical optimal frequency
void optimalFrequency()
{
	double minERROR = 100;
	int optimalDIFF;

	startFreq1 = 100000000;
	startFreq2 = 100000960;

	for (int i = 1; i < 4; ++i)
	{
		//printf("sf1 = %d and sf2 = %d and error = ", startFreq1, startFreq2);
		sampling();
		FFT(time1source, freq1, 1, numPoints, 1);
		FFT(time2source, freq2, 1, numPoints, 1);

		int shiftedPoints = shiftFreqHigher(freq1, freq2);
		FFT(freq1, timesent, 1, shiftedPoints, -1);
		for (int i = 0; i < shiftedPoints; ++i)
		{
			timesent[i].a  /= (double)shiftedPoints; 
			timesent[i].b  /= (double)shiftedPoints;
		}

		FFT(timesent, freq1, 1, shiftedPoints, 1);
		shiftFreqLower(freq1, freq2);

		FFT(freq1, time1dest, 1, numPoints, -1);
		FFT(freq2, time2dest, 1, numPoints, -1);

		for (int i = 0; i < numPoints; ++i)
		{
			time1dest[i].a  /= numPoints;
			time2dest[i].a  /= numPoints;  
		}

		//Computing ERROR_RMS
		double ERROR_RMS1 = 0.0, ERROR_RMS2 = 0.0;
		for (int i = 0; i < numPoints; ++i)
		{
			ERROR_RMS1 += (mod(time1source[i])-mod(time1dest[i]))*(mod(time1source[i])-mod(time1dest[i]));
			ERROR_RMS2 += (mod(time2source[i])-mod(time2dest[i]))*(mod(time2source[i])-mod(time2dest[i]));
		}
		ERROR_RMS1 /= numPoints;ERROR_RMS2 /= numPoints;
		ERROR_RMS1 = sqrt(ERROR_RMS1);ERROR_RMS2 = sqrt(ERROR_RMS2);

		double meanERROR = (ERROR_RMS1 + ERROR_RMS2)/2;
		if(meanERROR < minERROR)
		{
			minERROR = meanERROR;
			optimalDIFF = startFreq2 - startFreq1;
		}
		//printf(" %lf\n", meanERROR);
		startFreq2 += 640;
	}
	printf("The experimental optimal difference between middle frequencies is %d\n", optimalDIFF);
	printf("The theoretical optimal difference between middle frequencies is %d\n", theoreticalOptimalFrequency());
}

//returns the theoretical optimal number of levels
int theoreticalOptimalLevels()
{
	int freqmax = calcFMax();
	int Bandwidth = calcBandwidth();
	double snr = (double)SourceAmp/NoiseAmp;
	snr = snr+1;
	snr = log2(snr);
	snr = snr * Bandwidth / (2 *freqmax);  //L = 2^(B/(2*fMax)*log2(1+SNR))
	return (int)pow(2, snr);
}

//computes experimental optimal number of levels and compares with theoretical optimal number of levels
void optimalLevels()
{
	double minERROR = 100;
	int optimalLevel;
	L = 5;

	for (int i = 1; i <6; ++i)
	{
		sampling();	
		FFT(time1source, freq1, 1, numPoints, 1);
		FFT(time2source, freq2, 1, numPoints, 1);
		int shiftedPoints = shiftFreqHigher(freq1, freq2);
		FFT(freq1, timesent, 1, shiftedPoints, -1);

		for (int i = 0; i < shiftedPoints; ++i)
		{
			timesent[i].a  /= (double)shiftedPoints; 
			timesent[i].b  /= (double)shiftedPoints;
		}


		Quantization(timesent, shiftedPoints);	
		noiseAddition(timesent, shiftedPoints);


		Quantization(timesent, shiftedPoints);
		FFT(timesent, freq1, 1, shiftedPoints, 1);
		shiftFreqLower(freq1, freq2);
		FFT(freq1, time1dest, 1, numPoints, -1);
		FFT(freq2, time2dest, 1, numPoints, -1);
		for (int i = 0; i < numPoints; ++i)
		{
			time1dest[i].a  /= numPoints;
			time2dest[i].a  /= numPoints;  
		}

		//Computing ERROR_RMS
		double ERROR_RMS1 = 0.0, ERROR_RMS2 = 0.0;
		for (int i = 0; i < numPoints; ++i)
		{
			ERROR_RMS1 += (mod(time1source[i])-mod(time1dest[i]))*(mod(time1source[i])-mod(time1dest[i]));
			ERROR_RMS2 += (mod(time2source[i])-mod(time2dest[i]))*(mod(time2source[i])-mod(time2dest[i]));
		}
		ERROR_RMS1 /= numPoints;ERROR_RMS2 /= numPoints;
		ERROR_RMS1 = sqrt(ERROR_RMS1);ERROR_RMS2 = sqrt(ERROR_RMS2);

		double meanERROR = (ERROR_RMS1 + ERROR_RMS2)/2;
		if(meanERROR < minERROR)
		{
			minERROR = meanERROR;
			optimalLevel = L;
		}
		//printf("Levels = %d and Error = %lf\n", L, meanERROR);

		L += 10;
	}
	printf("Experimental Optimal number of levels is %d\n", optimalLevel);
	printf("Theoretical Optimal number of levels is %d\n", theoreticalOptimalLevels());
}

//Part 1 Analog transmission over a noiseless channel
void part1()
{
	startFreq1 = 100000000;
	startFreq2 = 100006400;

	int shiftedPoints = processingAtSource();
	
	plotGraph((char *)("Sent signal"), timesent, shiftedPoints, 100);

	processingAtDest(shiftedPoints);

	optimalFrequency();

	printf("\n\n");
}

//Part 2 Analog transmission over a noisy channel
void part2()
{
	startFreq1 = 100000000;
	startFreq2 = 100006400;

	int shiftedPoints = processingAtSource();
	
	noiseAddition(timesent, shiftedPoints);
	
	plotGraph((char *)("Sent signal"), timesent, shiftedPoints, 100);

	processingAtDest(shiftedPoints);

	printf("\n\n");
}

//Part 3 Digital transmission over a noisy channel
void part3()
{
	startFreq1 = 100000000;
	startFreq2 = 100006400;

	int shiftedPoints = processingAtSource();
	
	L = 100;
	Quantization(timesent, shiftedPoints);
	
	noiseAddition(timesent, shiftedPoints);

	plotGraph((char *)("Sent signal"), timesent, shiftedPoints, 100);

	//At destination
	
	L = 100;
	Quantization(timesent, shiftedPoints);

	plotGraph((char *)("Received signal"), timesent, shiftedPoints, 100);

	processingAtDest(shiftedPoints);

	optimalLevels();

	printf("\n\n");
}

//main function
int main()
{
	srand((unsigned int)time(NULL));  //seeding randomize function with time

	int choice;
	do
	{
		printf("Enter 1 for part1 , 2 for part2 , 3 for part3 , -1 to exit\n");
		scanf("%d",&choice);

		switch(choice)
		{
			case 1:
				part1();  //for part 1
				break;

			case 2:
				part2();  //for part 2
				break;

			case 3:
				part3();  //for part 3
				break;
		}
	}while(choice!=-1);
	
	return 0;
}