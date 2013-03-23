package uk.co.demon.kerofin.rqhelper;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class SelectorActivity extends Activity {
	private static final String TAG="RQHelperSelector";
	
	static class RQ3ButtonHelper implements OnClickListener {
		SelectorActivity selector;
		
		RQ3ButtonHelper(SelectorActivity s, Button button) {
			selector=s;
			button.setOnClickListener(this);
		}

		@Override
		public void onClick(View view) {
			selector.switchToRQ3();
		}
		
	}

	static class RQ6ButtonHelper implements OnClickListener {
		SelectorActivity selector;
		
		RQ6ButtonHelper(SelectorActivity s, Button button) {
			selector=s;
			button.setOnClickListener(this);
		}

		@Override
		public void onClick(View view) {
			selector.switchToRQ6();
		}
		
	}

	Button rq3_button, rq6_button;
	RQ3ButtonHelper rq3_helper;
	RQ6ButtonHelper rq6_helper;
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_selector);
        
        rq3_button=(Button) findViewById(R.id.rq3_button);
        rq3_helper=new RQ3ButtonHelper(this, rq3_button);
        rq6_button=(Button) findViewById(R.id.rq6_button);
        rq6_helper=new RQ6ButtonHelper(this, rq6_button);
        
    }

	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.activity_selector, menu);
        return true;
    }
    
    public void switchToRQ3() {
    	Log.d(TAG, "switch to RQ3");
    	startActivity(new Intent(this, RQ3Activity.class));
    }
    
    public void switchToRQ6() {
    	Log.d(TAG, "switch to RQ6");
    	
    }
}
