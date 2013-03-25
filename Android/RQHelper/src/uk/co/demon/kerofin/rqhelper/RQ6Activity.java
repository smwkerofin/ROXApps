package uk.co.demon.kerofin.rqhelper;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

public class RQ6Activity extends Activity 
	implements TextWatcher, OnItemSelectedListener {
	private static final String TAG="RQ6Activity";
	
	EditText skill_per;
	TextView crit, succ, fail, fumb, modskill;
	Spinner difficulty;
	
	RQ6SkillRanges range;
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_rq6);
        
        int value=50;
        skill_per=(EditText) findViewById(R.id.rq6_skill_per);
        skill_per.setText(""+value);
    	skill_per.addTextChangedListener(this);
        
        crit=(TextView) findViewById(R.id.rq6_critical);
        succ=(TextView) findViewById(R.id.rq6_success);
        fail=(TextView) findViewById(R.id.rq6_failure);
        fumb=(TextView) findViewById(R.id.rq6_fumble);
        modskill=(TextView) findViewById(R.id.rq6_modskill);
        
        difficulty=(Spinner) findViewById(R.id.rq6_difficulty);
        difficulty.setSelection(RQ6Difficulty.convertFrom(RQ6Difficulty.Level.STANDARD));
        difficulty.setOnItemSelectedListener(this);
        
        range=new RQ6SkillRanges(value);
        
        recalculate();
    }
    
    private void recalculate() {
    	Log.d(TAG, "recalculate");
    	
    	String s=skill_per.getText().toString();
    	try {
    		int skill=Integer.parseInt(s);
    		
    		
    		Log.d(TAG, "skill="+skill);
    	} catch(NumberFormatException e) {
    		Log.e(TAG, "failed to parse "+s, e);
    		return;
    	}
    	
    	modskill.setText(""+range.get());
    	
    	crit.setText(range.getCritical().toString());
    	succ.setText(range.getSuccess().toString());
    	fail.setText(range.getFailure().toString());
    	fumb.setText(range.getFumble().toString());

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

	@Override
	public void onItemSelected(AdapterView<?> arg0, View arg1, int arg2,
			long arg3) {
		// TODO Auto-generated method stub
		RQ6Difficulty diff=
				RQ6Difficulty.fromInteger(difficulty.getSelectedItemPosition());
		range.set(diff);
		recalculate();
	}

	@Override
	public void onNothingSelected(AdapterView<?> arg0) {
		// TODO Auto-generated method stub
		range.set(RQ6Difficulty.Level.STANDARD);
		recalculate();
		
	}
}
