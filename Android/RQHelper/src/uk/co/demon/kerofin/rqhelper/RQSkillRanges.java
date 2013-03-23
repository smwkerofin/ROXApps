package uk.co.demon.kerofin.rqhelper;

public abstract class RQSkillRanges {
	protected int skill;
	protected PercentileRange crit, spec, succ, fail, fumb;

	RQSkillRanges(int sk) {
		crit=new PercentileRange();
		spec=new PercentileRange();
		succ=new PercentileRange();
		fail=new PercentileRange();
		fumb=new PercentileRange();
		
		set(sk);
	}
	
	public void set(int sk) {
		skill=sk;
		calculate();
	}
	
	public PercentileRange getCritical() {
		return crit;
	}
	
	public PercentileRange getSpecial() {
		return spec;
	}
	
	public PercentileRange getSuccess() {
		return succ;
	}
	
	public PercentileRange getFailure() {
		return fail;
	}
	
	public PercentileRange getFumble() {
		return fumb;
	}
	
	abstract void calculate();
}
