package uk.co.demon.kerofin.rmrolling;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.widget.EditText;
import android.widget.TextView;

public class NewStatActivity extends Activity implements TextWatcher {
	private static final String TAG="NewStatActivity";
	
	private EditText initial_value, potential_roll;
	private TextView potential_value;
	
	private StatPotential stat_pot;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_new_stat);
        
        initial_value=(EditText) findViewById(R.id.new_stat_initial);
        initial_value.addTextChangedListener(this);
        potential_roll=(EditText) findViewById(R.id.new_stat_pot);
        potential_roll.addTextChangedListener(this);
        
        potential_value=(TextView) findViewById(R.id.new_stat_value);

        try {
        	stat_pot=StatPotential.get();
        } catch(StatPotential.InvalidFile ex) {
        	Log.e(TAG, "no potentials: "+ex);
        }
        
        potential_value.setText("");
                               
        recalculate();
   }
    
    private void recalculate() {
    	int val=getValue(initial_value);
    	int roll=getValue(potential_roll);
    	
    	Log.d(TAG, "val "+val+" roll "+roll);
    	
    	if(val>0 && roll>0) {
    		int pot=stat_pot.getPotential(val, roll);
    		Log.d(TAG, "pot="+pot);
    	
    		setValue(pot);
    	}
    	else
    	{
            potential_value.setText("");
    		
    	}
    }

    private int getValue(EditText widget) {
    	String s=widget.getText().toString();
    	if(s.equals("")) {
    		return 0;
    	}
    	return Integer.parseInt(s);
    }
    
    private void setValue(int value) {
    	potential_value.setText(""+value);
    }
    
	@Override
	public void afterTextChanged(Editable s) {
		recalculate();
		
	}

	@Override
	public void beforeTextChanged(CharSequence s, int start, int count,
			int after) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onTextChanged(CharSequence s, int start, int before, int count) {
		// TODO Auto-generated method stub
		
	}
}
