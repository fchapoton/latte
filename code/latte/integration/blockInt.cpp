#define COEFF_MAX 10000

#include "PolyRep.h"
#include "newIntegration.h"
#include "../timing.h"
#include <iostream>
#include <fstream>
#include <NTL/ZZ.h>

using namespace std;

int main(int argc, char *argv[])
{
	if (argc < 4) {cout<<"Usage: "<< argv[0] <<" fileIn fileOut IntegrateMode (polynomial/linear)" <<endl; return 1;}; //usage of the function
	_linFormSum forms;
	_monomialSum myPoly;
	simplexZZ mySimplex;
	ifstream myStream (argv[1]);
	ofstream hisStream (argv[2]);
	if (!myStream.is_open()) {cout<<"error opening file "<< argv[1] <<",please make sure it is spelled correctly!"<<endl; return 1; };
	ZZ a,b;
	string line,line2;
	int polyCount=0;
	Timer myTimer("Integration timer");
	float loadTime, newTime, lastTime;
        loadTime=0;

	while (!myStream.eof())
	{
		getline(myStream,line);delSpace(line);if (line.length()==0) break;
		getline(myStream,line2);delSpace(line2);
		polyCount++;
		if (!strcmp(argv[3],"polynomial")) 
		{
			a=0;b=0;
			_loadMonomials(myPoly, line);
			convertToSimplex(mySimplex, line2);
			lastTime = myTimer.get_seconds();
			myTimer.start();
			_integrateMonomialSum(a, b, myPoly, mySimplex);
			hisStream<<a<<endl<<b<<endl;
			myTimer.stop();	
			newTime = myTimer.get_seconds();
			FILE* markFile = fopen("integration/mark.txt","w");
			if (newTime-lastTime>600) 
			{
				FILE* myFile = fopen("integration/burstbench1.txt","a");
				//timeout
				fprintf(myFile, "%10.4f", newTime-lastTime);
				fclose(myFile);
				fprintf(markFile, "1");
				fclose(markFile);
				exit(0);
			}
			else loadTime += (newTime - lastTime);	
		}
		else if (!strcmp(argv[3],"linear"))
		{
			a=0;b=0;
			_loadLinForms(forms, line);
			convertToSimplex(mySimplex, line2);
			lastTime = myTimer.get_seconds();
			myTimer.start();
			_integrateLinFormSum(a, b, forms, mySimplex);
			hisStream<<a<<endl<<b<<endl;
			myTimer.stop();
			newTime = myTimer.get_seconds();
			FILE* markFile = fopen("integration/mark.txt","w");
			if (newTime-lastTime>10) 
			{
				FILE* myFile = fopen("integration/burstbench1.txt","a");
				fprintf(myFile, "%10.4f", newTime-lastTime);
				//timeout
				fclose(myFile);
				fprintf(markFile, "1");
				fclose(markFile);
				exit(0);
			}
			else loadTime += (newTime - lastTime);	
		};
	};
	myStream.close();
	hisStream.close();

	loadTime = loadTime/polyCount;
	//writes average integration time to file burstbench1.txt
	FILE* myFile = fopen("integration/burstbench1.txt","a");
        if (loadTime<0) loadTime=-loadTime;
	fprintf(myFile, "%10.4f", loadTime);
	fclose(myFile);
	//time is not out
        FILE* markFile=fopen("integration/mark.txt","w");
	fprintf(markFile, "0");
	fclose(markFile);
	return 0;
}
