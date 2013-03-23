package uk.co.demon.kerofin.rqhelper;

public class PercentileValue {
	int value;
	
	PercentileValue() {
		value=0;
	}
	
	PercentileValue(int v) {
		value=0;
		set(v);
	}
	
	public void set(int v) {
		if(v>=1 && v<=100) {
			value=v;
		}
	}
	
	public int get() {
		return value;
	}
	
	public boolean isValid() {
		return value>=1 && value<=100;
	}
	
	public String toString() {
		if(!isValid()) 
			return "  ";
		
		if(value==100)
			return "00";
		
		if(value>9)
			return ""+value;
		
		return "0"+value;
	}
}
