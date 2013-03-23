package uk.co.demon.kerofin.rqhelper;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.widget.EditText;
import android.widget.TextView;

public class RQ3Activity extends Activity implements TextWatcher {
	private static final String TAG="RQ3Activity";
	
	EditText skill_per;
	TextView crit, spec, succ, fail, fumb;
	
	RQ3SkillRanges range;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_rq3);
        
        skill_per=(EditText) findViewById(R.id.rq3_skill_per);
    	skill_per.addTextChangedListener(this);
        
        crit=(TextView) findViewById(R.id.rq3_crit);
        spec=(TextView) findViewById(R.id.rq3_spec);
        succ=(TextView) findViewById(R.id.rq3_success);
        fail=(TextView) findViewById(R.id.rq3_failure);
        fumb=(TextView) findViewById(R.id.rq3_fumble);
        
        range=new RQ3SkillRanges(50);
        
    }
    
    private void recalculate() {
    	Log.d(TAG, "recalculate");
    	
    	String s=skill_per.getText().toString();
    	try {
    		int skill=Integer.parseInt(s);
    		
    		range.set(skill);
    		Log.d(TAG, "skill="+skill);
    	} catch(NumberFormatException e) {
    		Log.e(TAG, "failed to parse "+s, e);
    	}
    	
    	crit.setText(range.getCritical().toString());
    	spec.setText(range.getSpecial().toString());
    	succ.setText(range.getSuccess().toString());
    	fail.setText(range.getFailure().toString());
    	fumb.setText(range.getFumble().toString());
    	Log.d(TAG, range.getFumble().toString());

    }

	@Override
	public void afterTextChanged(Editable s) {
		// TODO Auto-generated method stub
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
