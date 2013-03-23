package uk.co.demon.kerofin.rqhelper;

public class RQ3SkillRanges extends RQSkillRanges {
	
	RQ3SkillRanges(int skill) {
		super(skill);
	}

	@Override
	void calculate() {
		// TODO Auto-generated method stub
		int c=skill/20;
		int r=skill%20;
		if(r>=10)
			c++;
		else if(c==0)
			c=1;
		
		crit.set(1, c);
		
		int s=skill/5;
		r=skill%5;
		if(r>=3)
			s++;
		else if(s==0)
			s=1;
		
		if(s>c)
			spec.set(c+1, s);
		else
			spec.unset();

		int n=skill;
		if(n>95)
			n=95;
		int l=s+1;
		if(s<=c)
			l=c+1;
		
		succ.set(l, n);
		
		int f=(100-skill)/20;
		r=(100-skill)%20;
		
		if(r>=10)
			f++;
		else if(f<=0)
			f=1;
		fumb.set(101-f, 100);
		 
		fail.set(n+1, 100-f);
	}

	public static void main(String args[]) {
		RQ3SkillRanges sk=new RQ3SkillRanges(1);
		for(int i=1; i<126; i++) {
			sk.set(i);
			System.out.println(i+": c="+sk.getCritical()+" s="+sk.getSpecial()+
					" n="+sk.getSuccess()+" f="+sk.getFailure()+
					" F="+sk.getFumble());
		}
	}
}
