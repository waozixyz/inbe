package xyz.waozi.inbe;

import android.widget.Toast;

final class GplayPaymentBridge {
    private GplayPaymentBridge() {}

    static String channel() {
        return "disabled";
    }

    static void purchaseTokens(final MainActivity activity, final String productId,
                               final String accountId, final String authToken) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(activity, "Token purchases are not available in this build.", Toast.LENGTH_LONG).show();
            }
        });
    }
}
