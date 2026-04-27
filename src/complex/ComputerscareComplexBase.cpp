/*
	ComputerscareComplexBase

	Compluterxare

	Extends Module with a polyChannels member variable,
	and a polyphony channels widget that is "bound" to the
	the module instance's value

*/


#pragma once

#include "math/ComplexMath.hpp"

using namespace rack;

struct ComputerscareComplexBase;


struct ComputerscareComplexBase : ComputerscareMenuParamModule {
	enum wrapModes {
        WRAP_NORMAL,
        WRAP_CYCLE,
        WRAP_MINIMAL,
        WRAP_STALL
  };
  enum portModes {
      RECT_INTERLEAVED,
      POLAR_INTERLEAVED,
      RECT_SEPARATED,
      POLAR_SEPARATED
  };

   std::vector<std::vector <int>> getInputCompolyphony(std::vector<int> inputModeIndices, std::vector<int> inputFirstPortIndices) {
   	std::vector<std::vector <int>> output;

   	int numInputsToConsider = inputFirstPortIndices.size();



		for(int i = 0; i < numInputsToConsider; i++) {
			std::vector<int> myStuff;
			int inputMode = params[inputModeIndices[i]].getValue();

			int portOnePolyphony = inputs[inputFirstPortIndices[i]].getChannels();
			int portTwoPolyphony = inputs[inputFirstPortIndices[i]+1].getChannels();
			
			if(inputMode == RECT_INTERLEAVED || inputMode == POLAR_INTERLEAVED) {
				int totalPolyphony = portOnePolyphony+portTwoPolyphony;
				myStuff.push_back((int) std::ceil((float) totalPolyphony/2));
			}
			else {
				//separated mode
				myStuff.push_back(std::max(portOnePolyphony,portTwoPolyphony));

			}

			myStuff.push_back(portOnePolyphony);
			myStuff.push_back(portTwoPolyphony);
			output.push_back(myStuff);
		}
		
   	return output;
   }

   void setOutputVoltages(int outIndex,int outMode,int compChannelIndex,float x, float y, float r, float theta) {
        int outputBlock = compChannelIndex > 7 ? 1 : 0;
        if(outMode==RECT_INTERLEAVED) {
            //interleaved rectangular
            outputs[outIndex+outputBlock].setVoltage(x,(compChannelIndex*2) % 16);
            outputs[outIndex+outputBlock].setVoltage(y,(compChannelIndex*2+1) % 16);
        } else if(outMode==POLAR_INTERLEAVED) {
            //interleaved polar
            outputs[outIndex+outputBlock].setVoltage(r,(compChannelIndex*2) % 16);
            outputs[outIndex+outputBlock].setVoltage(theta,(compChannelIndex*2+1) % 16);
        } else if(outMode==RECT_SEPARATED) {
            //separated rectangular
            outputs[outIndex].setVoltage(x,compChannelIndex);
            outputs[outIndex+1].setVoltage(y,compChannelIndex);
        } else if(outMode==POLAR_SEPARATED) {
            //separated polar
            outputs[outIndex].setVoltage(r,compChannelIndex);
            outputs[outIndex+1].setVoltage(theta,compChannelIndex);
        }
    }

    void setOutputChannels(int outIndex,int outMode,int compolyChannels) {
        if(outMode==RECT_INTERLEAVED || outMode ==POLAR_INTERLEAVED) {
            //interleaved
            int numTotalPolyChannels = compolyChannels*2;
            int numChannels1 = numTotalPolyChannels >= 16 ? 16 : numTotalPolyChannels;
            int numChannels2 = numTotalPolyChannels >= 16 ? numTotalPolyChannels-16 : 0;

            outputs[outIndex+0].setChannels(numChannels1);
            outputs[outIndex+1].setChannels(numChannels2);
        } else {
            outputs[outIndex+0].setChannels(compolyChannels);
            outputs[outIndex+1].setChannels(compolyChannels);
        }
    }

	int calcOutputCompolyphony(int knobSetting,std::vector<std::vector <int>> inputCompolyphonyChannels) {
		int numInputsToConsider = inputCompolyphonyChannels.size();

		int outputCompolyphony;

		int maxOfInputsCompolyphony = 0;

		for(int i = 0; i < numInputsToConsider; i++) {
			maxOfInputsCompolyphony= std::max(maxOfInputsCompolyphony,inputCompolyphonyChannels[i][0]);
		}

		//automatic, use max of input channels compolyphony
		if(knobSetting == 0) {
			if(maxOfInputsCompolyphony == 0) {
				outputCompolyphony = 1;
			} else {
				outputCompolyphony = maxOfInputsCompolyphony;
			}
		} else {
			outputCompolyphony = knobSetting;
		}

		return outputCompolyphony;
	}

	/*
		S

		Separated: 
			Complex 0:
				- port0, ch0
				- port1, ch0
			
			Complex 1:
				- port0, ch1
				- port1, ch1


		Interleaved:
			Complex 0:
				-port0, ch0
				-port0, ch1

			Complex 1:
				-port0, ch2
				-port0, ch3
	*/

	std::vector<float> getComplexVoltageFromSeparatedInput(int outputIndex, int firstPortID, int inputMode,int wrapMode,std::vector <int>&  inputCompolyphony) {
		std::vector<float> output = {};

			int firstPortIndex = outputIndex;
			int secondPortIndex = outputIndex;
			if(wrapMode == WRAP_NORMAL) {
	    	/*
          If monophonic, copy ch1 to all
          Otherwise use the poly channels
        */
	    	if(inputCompolyphony[1] == 1) {
	    		firstPortIndex = 0;
	    	} else {
	    		firstPortIndex = outputIndex;
	    	}
	    	if(inputCompolyphony[2] == 1) {
	    		secondPortIndex = 0;
	    	} else {
	    		secondPortIndex = outputIndex;
	    	}
     } 
	  	output.push_back(inputs[firstPortID].getVoltage(firstPortIndex));
	  	output.push_back(inputs[firstPortID+1].getVoltage(secondPortIndex));

	   return output;
	}
	std::vector<float> getComplexVoltageFromInterleavedInput(int outputIndex, int firstPortID, int inputMode,int wrapMode,std::vector<int>& inputCompolyphony) {
		std::vector<float> output = {};
		int portIndex = outputIndex >= 8 ? firstPortID + 1 : firstPortID;
		int relativeOutputChannelIndex = outputIndex % 8;
		int firstChannelIndex=0;


		if(wrapMode == WRAP_NORMAL) {
	    	/*
          If monophonic, copy ch1 to all
          Otherwise use the poly channels
        */
	    	if(inputCompolyphony[0] == 2) {
	    		firstChannelIndex = 0;
	    	} else {
	    		firstChannelIndex = relativeOutputChannelIndex*2;
	    	}
     } 
	   output.push_back(inputs[portIndex].getVoltage(firstChannelIndex));
	   output.push_back(inputs[portIndex].getVoltage(firstChannelIndex+1));
	   return output;
	}

	std::vector<float> getQuad(std::vector<float>& ab,int type) {
		cpx::complex_math::Quad z = cpx::complex_math::quadFromPair(
			ab[0], ab[1], static_cast<cpx::complex_math::CoordinateMode>(type));
		return {z.x,z.y,z.r,z.theta};
	}
			




	std::vector<float>  getComplexVoltage(int outputIndex, int firstPortID, int inputMode,int wrapMode,std::vector<std::vector <int>>& inputCompolyphony) {
		//7% -> 3%
		//return {0.f,1.f,2.f,3.f};
		std::vector<float> mainInputVoltages;
		if(inputMode==RECT_INTERLEAVED || inputMode == POLAR_INTERLEAVED) { 
			mainInputVoltages = getComplexVoltageFromInterleavedInput(outputIndex, firstPortID,inputMode, wrapMode, inputCompolyphony[0]);
		} else {
			mainInputVoltages = getComplexVoltageFromSeparatedInput(outputIndex, firstPortID,inputMode, wrapMode, inputCompolyphony[0]);
		}
		return getQuad(mainInputVoltages,inputMode);
	}

	std::vector<int> getChannelIndicesFromSeparatedInput(int outputIndex, int wrapMode, std::vector<int> channelCounts) {
        std::vector<int> output;
        for(int i = 0; i < channelCounts.size(); i++) {
        	int myInputChannelIndex=0;;
        	int myNumChannels = channelCounts[i];
    	    if(wrapMode == WRAP_NORMAL) {
    	    	/*
              If monophonic, copy ch1 to all
              Otherwise use the poly channels
            */
    	    	if(myNumChannels == 1) {
    	    		myInputChannelIndex = 0;
    	    	} else {
    	    		myInputChannelIndex = outputIndex;
    	    	}
	        } else if(wrapMode == WRAP_CYCLE) {
	        	// Cycle through the poly channels
	        	myInputChannelIndex = outputIndex % myNumChannels;
	        } else if(wrapMode == WRAP_MINIMAL) {
	          // Do not copy channel 1 if monophonic
	        	myInputChannelIndex = outputIndex;
	        } else if(wrapMode == WRAP_STALL) {
	          // Go up to the maximum channel, and then use the final value for the rest
	        	myInputChannelIndex = outputIndex>myNumChannels-1 ? myNumChannels-1 : outputIndex;
	        }
	        //reverse
	        //pingpong
	        //random shuffled

	        output.push_back(myInputChannelIndex);
        }
        return output;
    }

};
