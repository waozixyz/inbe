package xyz.waozi.inbe;

import android.util.Log;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

public class FcmPushService extends FirebaseMessagingService {
    private static final String TAG = "INBE_FCM";

    @Override
    public void onNewToken(String token) {
        Log.d(TAG, "new FCM token");
    }

    @Override
    public void onMessageReceived(RemoteMessage message) {
        Log.d(TAG, "FCM message received");
    }
}
