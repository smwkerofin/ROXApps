package uk.co.demon.kerofin.rmrolling;

import java.io.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import org.xml.sax.*;

import android.content.res.AssetManager;

public class BonusSet {
	private static final int MAX=100;
	private static BonusSet bonus_set=null;
	
	public static class InvalidFile extends Exception {
		/**
		 * 
		 */
		private static final long serialVersionUID = 4051748318471024067L;
		private String reason;
		
		public InvalidFile(String Reason)
		{
			reason=Reason;
		}
		
		public String toString() {
			return "invalid bonus set file: "+reason;
		}
	}
	
	private class Bonus {
		@SuppressWarnings("unused")
		public int value;
		public int bonus;
		public double devp, pp;
		
		public Bonus(int Value, int Bonus, double DevP, double PP) {
			value=Value;
			bonus=Bonus;
			devp=DevP;
			pp=PP;
		}
		
		public Bonus() {
			value=0;
			bonus=-30;
			devp=0.0;
			pp=0.0;
		}
	}
	
	Bonus[] bonuses;

	BonusSet(Document xml) throws InvalidFile {
		bonuses=new Bonus[MAX+1];
		bonuses[0]=new Bonus();
		
		readFrom(xml);
	}
	
	private void readFrom(Document xml) throws InvalidFile {
		Element root=xml.getDocumentElement();
		if(!root.getTagName().equals("BonusSet"))
		{
			throw new InvalidFile("root tag must be BonusSet, not "+root.getTagName());
		}
		
		NodeList children=root.getChildNodes();
		for(int i=0; i<children.getLength(); i++) {
			Node child=children.item(i);
			
			if(child.getNodeType()==Node.ELEMENT_NODE) {
				Element el=(Element) child;
				if(el.getTagName().equals("Bonus")) {
					int val=Integer.parseInt(el.getAttribute("Value"));
					int bonus=Integer.parseInt(el.getAttribute("Bonus"));
					double devp=Double.parseDouble(el.getAttribute("DevPt"));
					double pp=Double.parseDouble(el.getAttribute("PP"));
					
					bonuses[val]=new Bonus(val, bonus, devp, pp);
				}
			}
		}
	}
	
	public int getBonus(int StatValue) {
		if(StatValue<0)
			return bonuses[0].bonus;
		else if(StatValue>MAX)
			return bonuses[MAX].bonus;
		return bonuses[StatValue].bonus;
	}
	
	public double getDevPoints(int StatValue) {
		if(StatValue<0)
			return bonuses[0].devp;
		else if(StatValue>MAX)
			return bonuses[MAX].devp;
		return bonuses[StatValue].devp;
	}
	
	public double getPowerPoints(int StatValue) {
		if(StatValue<0)
			return bonuses[0].pp;
		else if(StatValue>MAX)
			return bonuses[MAX].pp;
		return bonuses[StatValue].pp;
	}
	
	public static BonusSet load(AssetManager assetManager) throws InvalidFile {
		
		if(bonus_set==null) {
			InputStream instream;
			
			try {
				instream = assetManager.open("companion1.xml");
			
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
			}
			
			try {
				DocumentBuilder doc_reader=
					DocumentBuilderFactory.newInstance().newDocumentBuilder();
				bonus_set=new BonusSet(doc_reader.parse(instream));
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
				
			} catch(ParserConfigurationException ex) {
				throw new InvalidFile("parser exception: "+ex);
				
			} catch(SAXException ex) {
				throw new InvalidFile("SAX exception: "+ex);
				
			}
		}
		
		return bonus_set;
	}
	
	public static BonusSet get() throws InvalidFile {
		
		if(bonus_set==null) {
			throw new InvalidFile("file not loaded");
		}
		
		return bonus_set;
	}
	

}
