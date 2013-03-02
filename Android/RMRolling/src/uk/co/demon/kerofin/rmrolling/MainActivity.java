package uk.co.demon.kerofin.rmrolling;

import android.os.Bundle;
import android.app.Activity;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.EditText;

public class MainActivity extends Activity implements TextWatcher, OnClickListener {
	private EditText rollText, obText, dbText, modText;
	private EditText resultText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        rollText=initEntry(R.id.rollText);
        obText=initEntry(R.id.obText);
        dbText=initEntry(R.id.dbText);
        modText=initEntry(R.id.modText);
        
        resultText=(EditText) findViewById(R.id.resultText);
        
        //rollText.selectAll();
                               
        recalculate();
   }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.activity_main, menu);
        return true;
    }
    
    private EditText initEntry(int id) {
    	EditText widget=(EditText) findViewById(id);
    	widget.addTextChangedListener(this);
    	widget.setOnClickListener(this);
    	return widget;
    }
    
    private int getValue(EditText widget) {
    	String s=widget.getText().toString();
    	return Integer.parseInt(s);
    }
    
    private void setValue(EditText widget, int value) {
    	widget.setText(""+value);
    }
    
    private void recalculate() {
    	try {
        	int val=getValue(rollText);
        	val+=getValue(obText);
        	val-=getValue(dbText);
        	val+=getValue(modText);
        	
        	setValue(resultText, val);
    		
    	} catch(NumberFormatException e) {
    		
    	}
    	
    }
    
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    	
    }
    
    public void onTextChanged(CharSequence s, int start, int count, int after) {
    }
    
    public void afterTextChanged(Editable widget) {
    	recalculate();
    }
    
    public void onClick(View view) {
    	EditText widget=(EditText) view;
    	widget.selectAll();
    }
}
