package uk.co.demon.kerofin.rqhelper;

public class PercentileRange {
	private PercentileValue low, high;
	
	PercentileRange() {
		low=new PercentileValue();
		high=new PercentileValue();
	}

	PercentileRange(int l, int h) {
		low=new PercentileValue(l);
		high=new PercentileValue(h);
	}

	public boolean isValid() {
		return low.isValid() && high.isValid() && high.get()>=low.get();
	}
	
	public void set(int l, int h) {
		low.set(l);
		high.set(h);
	}
	
	public void unset() {
		low.set(2);
		high.set(1);
	}
	
	public String toString() {
		if(!isValid())
			return "  -  ";
		
		return low.toString()+"-"+high.toString();
	}
}
