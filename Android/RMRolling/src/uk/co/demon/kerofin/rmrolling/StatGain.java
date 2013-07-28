package uk.co.demon.kerofin.rmrolling;

import java.io.*;

import javax.xml.parsers.*;

import org.w3c.dom.*;
import org.xml.sax.*;

import android.content.res.AssetManager;
import android.util.Log;


public class StatGain {
	private static final String TAG="StatGain";
	private static final int MAX_DIFF=15+1;
	private static final int MAX_ROLL=100;
	
	private static StatGain stat_gain=null;
	
	public static class InvalidFile extends Exception {
		/**
		 * 
		 */
		private static final long serialVersionUID = -4155195846038732620L;
		private String reason;
		
		public InvalidFile(String Reason)
		{
			reason=Reason;
		}
		
		public String toString() {
			return "invalid stat gain file: "+reason;
		}
	}
	

	private class Difference {
		@SuppressWarnings("unused")
		public int diff;
		private int gains[];
		
		public Difference(int Value) {
			diff=Value;
			gains=new int[MAX_ROLL];
		}
		
		public void setGain(int Roll, int Gain) {
			if(Roll>=1 && Roll<=MAX_ROLL) {
				gains[Roll-1]=Gain;
			}
		}
		
		public int getGain(int Roll) {
			if(Roll<1) {
				return -2;
			} else if(Roll<=4) {
				return -2*Roll;
			} else if(Roll>MAX_ROLL) {
				return gains[MAX_ROLL-1];
			}
			
			return gains[Roll-1];
		}
	}
	
	Difference diffs[];
	
	StatGain(Document xml) throws InvalidFile {
		diffs=new Difference[MAX_DIFF];
		
		for(int i=0; i<MAX_DIFF; i++) {
			diffs[i]=new Difference(i);
		}
		
		readFrom(xml);
	}
	
	private void readFrom(Document xml) throws InvalidFile {
		Element root=xml.getDocumentElement();
		if(!root.getTagName().equals("StatGains"))
		{
			throw new InvalidFile("root tag must be StatGains, not "+root.getTagName());
		}
		
		NodeList children=root.getChildNodes();
		for(int i=0; i<children.getLength(); i++) {
			Node child=children.item(i);
			
			if(child.getNodeType()==Node.ELEMENT_NODE) {
				Element el=(Element) child;
				if(el.getTagName().equals("Difference")) {
					int diff=Integer.parseInt(el.getAttribute("Value"));
					if(diff<0 || diff>=MAX_DIFF) {
						continue;
					}
					
					NodeList child2=el.getChildNodes();
					for(int j=0; j<child2.getLength(); j++) {
						Node sub=child2.item(j);
						if(sub.getNodeType()==Node.ELEMENT_NODE) {
							Element subel=(Element) sub;
							if(subel.getTagName().equals("Gain")) {
								if(subel.getAttribute("Roll").equals("")) {
									Log.e(TAG, subel.getAttribute("Roll")+" "+
												diff+" "+el);
								}
									
								int roll=Integer.parseInt(subel.getAttribute("Roll"));
								String sgain=subel.getAttribute("Gain");
								int gain;
								if(sgain.equals("*")) {
									gain=-2*roll;
								} else {
									gain=Integer.parseInt(sgain);
								}
								
								diffs[diff].setGain(roll, gain);
							}
						}
					}
				}
			}
		}
	}
	
	public int getGain(int Diff, int Roll) {
		if(Diff<0) {
			return diffs[0].getGain(Roll);
		} else if(Diff>=MAX_DIFF) {
			return diffs[MAX_DIFF-1].getGain(Roll);
		}
		
		return diffs[Diff].getGain(Roll);
	}

	public static StatGain load(AssetManager assetManager) throws InvalidFile {
		
		if(stat_gain==null) {
			InputStream instream;
			
			try {
				instream = assetManager.open("stat_gain.xml");
			
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
			}
			
			try {
				DocumentBuilder doc_reader=
					DocumentBuilderFactory.newInstance().newDocumentBuilder();
				stat_gain=new StatGain(doc_reader.parse(instream));
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
				
			} catch(ParserConfigurationException ex) {
				throw new InvalidFile("parser exception: "+ex);
				
			} catch(SAXException ex) {
				throw new InvalidFile("SAX exception: "+ex);
				
			}
		}
		
		return stat_gain;
	}
	
	public static StatGain get() throws InvalidFile {
		
		if(stat_gain==null) {
			throw new InvalidFile("file not loaded");
		}
		
		return stat_gain;
	}
	
}
