package uk.co.demon.kerofin.rqhelper;

public class RQ6Difficulty {
	public enum Level {
		AUTOMATIC, VERY_EASY, EASY, STANDARD,
		HARD, FORMIDABLE, HERCULEAN, HOPELESS
	}
	
	static final String[] names={
		"Automatic", "Very easy", "Easy", "Standard",
		"Hard", "Formidable", "Herculean", "Hopeless"
	};
	
	Level level;
	
	RQ6Difficulty(Level l) {
		level=l;
	}
	
	public Level get() {
		return level;
	}
	
	static Level convertTo(int i) {
		assert(i<0 || i>=8);
		
		Level level=Level.STANDARD;
		switch(i) {
		case 0:
			level=Level.AUTOMATIC;
			break;
		case 1:
			level=Level.VERY_EASY;
			break;
		case 2:
			level=Level.EASY;
			break;
		case 3:
			level=Level.STANDARD;
			break;
		case 4:
			level=Level.HARD;
			break;
		case 5:
			level=Level.FORMIDABLE;
			break;
		case 6:
			level=Level.HERCULEAN;
			break;
		case 7:
			level=Level.HOPELESS;
			break;
		}
		
		return level;
	}
	
	static int convertFrom(Level level) {
		int i=3;
		switch(level) {
		case AUTOMATIC:
			return 0;
		case VERY_EASY:
			return 1;
		case EASY:
			return 2;
		case STANDARD:
			return 3;
		case HARD:
			return 4;
		case FORMIDABLE:
			return 5;
		case HERCULEAN:
			return 6;
		case HOPELESS:
			return 7;
		}
		return i;
	}
	
	public static RQ6Difficulty fromInteger(int i) {
		Level level=convertTo(i);
		return new RQ6Difficulty(level);
	}
	
	public int toInteger() {
		return convertFrom(level);
	}
	
	public int modifySkill(int skill) {
		int tmp=0;
		
		switch(level) {
		case AUTOMATIC:
			return skill<100? 200: skill*2;
			
		case VERY_EASY:
			return skill*2;
			
		case EASY:
			tmp=skill*3;
			return (tmp%2>0)? tmp/2+1: tmp/2;
			
		case STANDARD:
			// Handle below
			break;
			
		case HARD:
			tmp=skill*2;
			return (tmp%3>0)? tmp/3+1: tmp/3;
			
		case FORMIDABLE:
			return (skill%2>0)? skill/2+1: skill/2;
			
		case HERCULEAN:
			return (skill%10>0)? skill/10+1: skill/10;
			
		case HOPELESS:
			return 0;
		}
		
		return skill;
	}
	
	public String toString() {
		return names[toInteger()];
	}
	
	public static void main(String args[]) {
		RQ6Difficulty diff;
		for(int i=0; i<8; i++) {
			diff=fromInteger(i);
			System.out.println(diff+":");
			for(int j=1; j<=125; j++) {
				System.out.println(" "+j+" -> "+diff.modifySkill(j));
			}
		}
	}
}
