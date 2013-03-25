package uk.co.demon.kerofin.rqhelper;

import uk.co.demon.kerofin.rqhelper.RQ6Difficulty.Level;

public class RQ6SkillRanges extends RQSkillRanges {
	RQ6Difficulty diff;
	int mod;

	RQ6SkillRanges(int sk) {
		super(sk);
		// TODO Auto-generated constructor stub
		
		diff=new RQ6Difficulty(Level.STANDARD);
		mod=diff.modifySkill(skill);
		calculate();
	}
	
	public int get() {
		return mod;
	}
	
	public void set(int sk) {
		if(diff!=null)
			mod=diff.modifySkill(sk);
		else
			mod=sk;
		super.set(sk);
	}
	
	public void set(RQ6Difficulty d) {
		diff=new RQ6Difficulty(d.get());
		mod=diff.modifySkill(skill);
		calculate();
	}
	
	public void set(Level level) {
		diff=new RQ6Difficulty(level);
		mod=diff.modifySkill(skill);
		calculate();
	}
	
	public void set(int sk, Level level) {
		diff=new RQ6Difficulty(level);
		set(sk);
	}
	
	public void set(int sk, RQ6Difficulty d) {
		diff=new RQ6Difficulty(d.get());
		set(sk);
	}
	

	@Override
	void calculate() {
		// TODO Auto-generated method stub
		int c=mod/10;
		int r=mod%10;
		if(r>0)
			c++;
		else if(c<=0)
			c=1;
		
		int s=mod;
		if(s>95)
			s=95;
		
		int f=99;
		if(mod>100)
			f++;
		
		crit.set(1, c);
		spec.unset();
		succ.set(c+1, s);
		fail.set(s+1, f-1);
		fumb.set(f, 100);

	}

	public static void main(String args[]) {
		RQ6SkillRanges sk=new RQ6SkillRanges(1);
		for(int j=1; j<=6; j++) {
			System.out.println(RQ6Difficulty.fromInteger(j));
			for(int i=1; i<126; i++) {
				sk.set(i, RQ6Difficulty.fromInteger(j));
				System.out.println(" "+i+" -> "+sk.get()+": c="+sk.getCritical()+
						" s="+sk.getSuccess()+" f="+sk.getFailure()+
						" F="+sk.getFumble());
			}
		}
	}
}
