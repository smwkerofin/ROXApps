package uk.co.demon.kerofin.rmrolling;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class SelectorActivity extends Activity {
	private static final String TAG="RMRollingSelector";
	
	static class RollingButtonHelper implements OnClickListener {
		SelectorActivity selector;
		
		RollingButtonHelper(SelectorActivity s, Button button) {
			selector=s;
			button.setOnClickListener(this);
		}

		@Override
		public void onClick(View view) {
			selector.switchToRolling();
		}
		
	}

	static class NewStatButtonHelper implements OnClickListener {
		SelectorActivity selector;
		
		NewStatButtonHelper(SelectorActivity s, Button button) {
			selector=s;
			button.setOnClickListener(this);
		}

		@Override
		public void onClick(View view) {
			selector.switchToNewStats();
		}
		
	}

	static class StatBonusButtonHelper implements OnClickListener {
		SelectorActivity selector;
		
		StatBonusButtonHelper(SelectorActivity s, Button button) {
			selector=s;
			button.setOnClickListener(this);
		}

		@Override
		public void onClick(View view) {
			selector.switchToStatBonus();
		}
		
	}

	Button rolling_button, new_stat_button, stat_bonus_button;
	RollingButtonHelper rolling_helper;
	NewStatButtonHelper new_stat_helper;
	StatBonusButtonHelper stat_bonus_helper;
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_selector);
        
        rolling_button=(Button) findViewById(R.id.rolling_button);
        rolling_helper=new RollingButtonHelper(this, rolling_button);
        new_stat_button=(Button) findViewById(R.id.stat_potentials);
        new_stat_helper=new NewStatButtonHelper(this, new_stat_button);
        stat_bonus_button=(Button) findViewById(R.id.stat_bonus);
        stat_bonus_helper=new StatBonusButtonHelper(this, stat_bonus_button);
        
        new_stat_button.setEnabled(false);
        
        try {
        	BonusSet.load(getApplication().getAssets());
        } catch(BonusSet.InvalidFile ex) {
        	Log.e(TAG, "cannot load bonus set: "+ex);
        	stat_bonus_button.setEnabled(false);
        }
        try {
        	StatGain.load(getApplication().getAssets());
        } catch(StatGain.InvalidFile ex) {
        	Log.e(TAG, "cannot load bonus set: "+ex);
        	stat_bonus_button.setEnabled(false);
        }
    }

    
    public void switchToRolling() {
    	Log.d(TAG, "switch to rolling");
    	startActivity(new Intent(this, RollingActivity.class));
    }
    
    public void switchToNewStats() {
    	Log.d(TAG, "switch to new stats");
    	//startActivity(new Intent(this, RQ6Activity.class));
    	
    }
    
    public void switchToStatBonus() {
    	Log.d(TAG, "switch to new bonus");
    	startActivity(new Intent(this, StatBonusActivity.class));
    	
    }

}
