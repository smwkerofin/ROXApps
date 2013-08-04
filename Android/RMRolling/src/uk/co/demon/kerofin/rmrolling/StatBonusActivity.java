package uk.co.demon.kerofin.rmrolling;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

public class StatBonusActivity extends Activity implements TextWatcher {
	private static final String TAG="StatBonusActivity";
	
	private EditText current_value, potential, imprv_roll;
	private TextView new_value, bonus, devp, pp;
	
	private BonusSet bonus_set;
	private StatGain stat_gain;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_stat_bonus);
        
        current_value=(EditText) findViewById(R.id.bonus_current);
        current_value.addTextChangedListener(this);
        potential=(EditText) findViewById(R.id.bonus_pot);
        potential.addTextChangedListener(this);
        imprv_roll=(EditText) findViewById(R.id.bonus_impr_roll);
        imprv_roll.addTextChangedListener(this);
        
        new_value=(TextView) findViewById(R.id.bonus_new_value);
        bonus=(TextView) findViewById(R.id.bonus_bonus);
        devp=(TextView) findViewById(R.id.bonus_dev_points);
        pp=(TextView) findViewById(R.id.bonus_pp);

        try {
        	bonus_set=BonusSet.get();
        } catch(BonusSet.InvalidFile ex) {
        	Log.e(TAG, "no bonus set: "+ex);
        }
        
        try {
        	stat_gain=StatGain.get();
        } catch(StatGain.InvalidFile ex) {
        	Log.e(TAG, "no stat gains: "+ex);
        }
        
        setValue(current_value, 50);
                               
        recalculate();
   }
    
    private void recalculate() {
    	int current=getValue(current_value);
    	//Log.d(TAG, "current "+current);
    	boolean update=false;
    	if(!imprv_roll.getText().toString().equals("")) {
    		update=true;
    	}
    	int pot=getValue(potential);
    	//Log.d(TAG, "potential "+pot+" update "+update);
    	
		int updated=current;
		if(update && pot>0) {
			int imprv=getValue(imprv_roll);
			//Log.d(TAG, "improvement roll "+imprv);
			
			if(stat_gain!=null) {
				int gain=stat_gain.getGain(pot-current, imprv);
				//Log.d(TAG, "gain "+gain);
				updated+=gain;
			}
			
    	}
		setValue(new_value, updated);
		
		if(bonus_set!=null) {
			setValue(bonus, bonus_set.getBonus(updated));
			setValue(devp, bonus_set.getDevPoints(updated));
			setValue(pp, bonus_set.getPowerPoints(updated));
		}
    }

    private int getValue(EditText widget) {
    	String s=widget.getText().toString();
    	if(s.equals("")) {
    		return 0;
    	}
    	return Integer.parseInt(s);
    }
    
    private void setValue(TextView widget, int value) {
    	widget.setText(""+value);
    }
    
     
    private void setValue(TextView widget, double value) {
    	widget.setText(""+value);
    }
    
     
	@Override
	public void afterTextChanged(Editable arg0) {
		recalculate();
		
	}

	@Override
	public void beforeTextChanged(CharSequence arg0, int arg1, int arg2,
			int arg3) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onTextChanged(CharSequence arg0, int arg1, int arg2, int arg3) {
		// TODO Auto-generated method stub
		
	}

}
