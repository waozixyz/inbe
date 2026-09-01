package xyz.waozi.inbe;

import android.widget.Toast;
import java.lang.reflect.Method;
import org.json.JSONObject;

final class PaymentBridge {
    private static final String DAOCHI_BASE_URL = "https://api.waozi.xyz";

    private PaymentBridge() {}

    static String channel() {
        String gplayChannel = gplayChannel();
        if (gplayChannel != null) {
            return gplayChannel;
        }
        return "monero";
    }

    static void purchaseTokens(final MainActivity activity, final String productId,
                               final String accountId, final String authToken) {
        if (purchaseGplayTokens(activity, productId, accountId, authToken)) {
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    String body = "{\"app_id\":\"inbe\",\"product_id\":\"" + jsonEscape(productId) + "\"}";
                    String response = SyncNetwork.httpRequest("INBE_PAYMENTS", "POST",
                            DAOCHI_BASE_URL + "/api/v1/tokens/purchases/monero/invoices",
                            body, authHeaders(authToken));
                    final Invoice invoice = parseInvoice(response);
                    activity.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (invoice.ok) {
                                Toast.makeText(activity,
                                        "Send " + invoice.atomicAmount + " atomic XMR to " + invoice.address,
                                        Toast.LENGTH_LONG).show();
                            } else {
                                Toast.makeText(activity, invoice.message, Toast.LENGTH_LONG).show();
                            }
                        }
                    });
                } catch (Exception e) {
                    showToast(activity, "Monero invoice failed");
                }
            }
        }, "inbe-monero-token-invoice").start();
    }

    private static String gplayChannel() {
        try {
            Method method = Class.forName("xyz.waozi.inbe.GplayPaymentBridge").getDeclaredMethod("channel");
            method.setAccessible(true);
            return (String) method.invoke(null);
        } catch (ClassNotFoundException e) {
            return null;
        } catch (Exception e) {
            return "disabled";
        }
    }

    private static boolean purchaseGplayTokens(MainActivity activity, String productId,
                                               String accountId, String authToken) {
        try {
            Method method = Class.forName("xyz.waozi.inbe.GplayPaymentBridge").getDeclaredMethod(
                    "purchaseTokens", MainActivity.class, String.class, String.class, String.class);
            method.setAccessible(true);
            method.invoke(null, activity, productId, accountId, authToken);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        } catch (Exception e) {
            showToast(activity, "Token purchases are not available in this build.");
            return true;
        }
    }

    private static String[] authHeaders(String authToken) {
        return new String[] {
                "Content-Type: application/json",
                "Authorization: Bearer " + (authToken != null ? authToken : "")
        };
    }

    private static Invoice parseInvoice(String response) throws Exception {
        int newline = response.indexOf('\n');
        int status = newline > 0 ? Integer.parseInt(response.substring(0, newline).trim()) : 0;
        String body = newline >= 0 ? response.substring(newline + 1) : "";
        if (status < 200 || status >= 300) {
            return new Invoice(false, "Monero invoice unavailable", "", 0);
        }
        JSONObject json = new JSONObject(body);
        return new Invoice(true, "", json.optString("address", ""),
                json.optLong("atomic_amount", 0));
    }

    private static void showToast(final MainActivity activity, final String message) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
            }
        });
    }

    private static String jsonEscape(String value) {
        if (value == null) return "";
        return value.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static final class Invoice {
        final boolean ok;
        final String message;
        final String address;
        final long atomicAmount;

        Invoice(boolean ok, String message, String address, long atomicAmount) {
            this.ok = ok;
            this.message = message;
            this.address = address;
            this.atomicAmount = atomicAmount;
        }
    }
}
