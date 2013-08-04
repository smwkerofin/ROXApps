package uk.co.demon.kerofin.rmrolling;

import java.io.*;

import javax.xml.parsers.*;

import org.w3c.dom.*;
import org.xml.sax.*;


import android.content.res.AssetManager;
//import android.util.Log;


public class StatPotential {
	//private static final String TAG="StatPotential";
	private static final int MAX_STAT=100;
	private static final int MAX_ROLL=100;
	
	private static StatPotential stat_pot=null;
	
	public static class InvalidFile extends Exception {
		/**
		 * 
		 */
		private static final long serialVersionUID = -3816195207966366768L;
		private String reason;
		
		public InvalidFile(String Reason)
		{
			reason=Reason;
		}
		
		public String toString() {
			return "invalid stat potential file: "+reason;
		}
	}
	
	private class Potentials {
		int potentials[];
		
		Potentials() {
			potentials=new int[MAX_ROLL+1];
			potentials[0]=0;
		}
		
		int getPotential(int Roll) {
			if(Roll<1) {
				return 0;
			} else if(Roll>MAX_ROLL) {
				return 101;
			}
			return potentials[Roll];
		}
		
		void setPotential(int Roll, int Potential) {
			if(Roll>=1 && Roll<=MAX_ROLL) {
				potentials[Roll]=Potential;
			}
		}
	}
	
	Potentials pots[];
	
	StatPotential() {
		pots=new Potentials[MAX_STAT+1];
	}
	
	StatPotential(Document xml) throws InvalidFile {
		pots=new Potentials[MAX_STAT+1];
		
		for(int i=1; i<=MAX_STAT; i++) {
			pots[i]=new Potentials();
		}
		
		readFrom(xml);
	}
	
	private void readFrom(Document xml) throws InvalidFile {
		Element root=xml.getDocumentElement();
		if(!root.getTagName().equals("Stats"))
		{
			throw new InvalidFile("root tag must be Stats, not "+root.getTagName());
		}
		
		NodeList children=root.getChildNodes();
		for(int i=0; i<children.getLength(); i++) {
			Node child=children.item(i);
			
			if(child.getNodeType()==Node.ELEMENT_NODE) {
				Element el=(Element) child;
				if(el.getTagName().equals("Roll")) {
					int roll_low=Integer.parseInt(el.getAttribute("Low"));
					int roll_high=Integer.parseInt(el.getAttribute("High"));
					
					NodeList child2=el.getChildNodes();
					for(int j=0; j<child2.getLength(); j++) {
						Node sub=child2.item(j);
						if(sub.getNodeType()==Node.ELEMENT_NODE) {
							Element subel=(Element) sub;
							if(subel.getTagName().equals("Stat")) {
									
								int stat_low=Integer.parseInt(subel.getAttribute("Low"));
								int stat_high=Integer.parseInt(subel.getAttribute("High"));
								String spot=subel.getAttribute("Potential");
								
								for(int stat=stat_low; 
										stat<=stat_high && stat<MAX_STAT; 
										stat++) {
									for(int roll=roll_low; roll<=roll_high; roll++) {
										int pot=stat;
										if(spot.equals("-")) {
											pot=stat;
										} else {
											pot=Integer.parseInt(spot);
										}
										
										pots[stat].setPotential(roll, pot);
										
									}
								}
									
							}
						}
					}
				}
			}
		}
	}
	
	public int getPotential(int Stat, int Roll) {
		if(Stat<=0) {
			return pots[0].getPotential(Roll);
		} else if(Stat>=MAX_STAT) {
			return pots[MAX_STAT-1].getPotential(Roll);
		}
		
		return pots[Stat].getPotential(Roll);
	}

	public static StatPotential load(AssetManager assetManager) throws InvalidFile {
		
		if(stat_pot==null) {
			InputStream instream;
			
			try {
				instream = assetManager.open("chlaw.xml");
			
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
			}
			
			try {
				DocumentBuilder doc_reader=
					DocumentBuilderFactory.newInstance().newDocumentBuilder();
				stat_pot=new StatPotential(doc_reader.parse(instream));
			} catch(IOException ex) {
				throw new InvalidFile("I/O exception: "+ex);
				
			} catch(ParserConfigurationException ex) {
				throw new InvalidFile("parser exception: "+ex);
				
			} catch(SAXException ex) {
				throw new InvalidFile("SAX exception: "+ex);
				
			}
		}
		
		return stat_pot;
	}
	
	public static StatPotential get() throws InvalidFile {
		
		if(stat_pot==null) {
			throw new InvalidFile("file not loaded");
		}
		
		return stat_pot;
	}
	

}
