package dev.unchartedg2.game;

import android.graphics.Bitmap;
import android.util.Base64;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.InflaterInputStream;

/** Anti-aliased action silhouettes from the user-supplied Uncharted control reference.
 * Kept as 4-bit alpha masks to minimise APK size; no fonts, emoji, or extra resources.
 * Classic PlayStation layout does not load any of these masks.
 */
final class UnchartedActionGlyphs {
    private static final int SIZE=64;
    private static final String[][] DATA={
        {"l1","eNrNlV1IVEEYhuestqJ7c1YzKCsUKiIStsQIg7Lowi6KNUKtLpLCH6w0qJT+tAKF7U/oQlIq6S4slCSKkjxF0EVkq91EIrvFJv2we+am3VbP2XmbOWvg2R3vHQ7nwHmYb97v+96ZIWQRDYU/m9zis8DILcyrrSgpdKtyvNbXevjFSFOrR84z2+obblGEe9sqpavn1hFyEgALDxyVRcj3ErKEApTil1vCKzxcIedA4pUnHbvP858ZfsHN0hIJ93GeY3E9EjiVzmtuE7XMis9fE+kB1p3dnDmW5GabVyLwWFWjoGD0j7RAec4yi89sj0l74M51WTzkSizQviQfz06oMlzdO3gtyPlsu4yTDUhg8u4oMAIm4es1LhyBqQ/w7EV5urhmIMKzZ9PHPVlIL5+znflfWtk/Uxx4k4rVhuuRsVGrtKhVh6KpPMfsGbjgB+PzA3H3vtnUAmnU8D3SrfQp2+liKQZw8v/hyMhHwYO4nJeaQLHoSoKONfElgphQ6BN7ctwWhpA2ee4m1REl3d/s829x04jckHjX248Y6bb7w1lyyeKiPj9WRELkiJ23mAf2DCU5EPhRTlZ+txWHs6tbg0lu+BEiBbH53MHl6Z8bLlk8PgRDKbAVMMMKbXZ1BZPz40rB1/k8S2TP0cP6APAFiCqu8fl89RxHuKc3+Izy/Lfb5i/RRGMsec+rqpCWv7Pt/hTmYrwrqvaHSMcDW3zjzLL/HKNPd3mUflv/OsAam5IVhqnhp6rAxvspo+8Pj1ruMihDeb7hTuUw7xykXGGYHw9Frqhij2/wyrCuOg3gFomT4k+2DbCDxTSx+uTuEwiAhdSOUza+CpwLfebpqn7Q146w3Z4ZfTE/ZbpYvnzLY6MwO2R3j6PTq0FohF72+9h+ci/FnZkYXjcoBLC/LWBe99EU8zv88YE6H+UH41vKedreU4dmgqzzIpd+CJiWnC3tce6gL3XXY906vOl7X9nIuY6p0x4/ZiXTleKoJno3U1PquyI7mpymLg7uh5gslF8d28B03WzWUSm/O5QKjenfNTqsLHR1rbkRaEBcVRe825Sly/u8Cllc4x+dWU3G"},
        {"l2","eNrNlc1rE0EUwGea1m7Aw0w8JWiJQYoga6oXQaF+4E2sOXivV/Fgj4JQY/AmQnvxpjH0D1jsqbfFg9cuCYpY2C5pe7BUXSi2qUlmn7O72e5kZrMXL32H8DK/eZP3HYROlEzMpPPCcjo//Smdn/2Yzs9VUyDh9qsI4REYo9KTdbZ2lYy4gJ/9BnDBXZtJvICfg9czNwyAjVtJ/DXs1c5fezV20YIfRRXfs7oVnp8Gz6HBvih43GEVPz4/P6fMnvIL9+FREH/T/8zCV9nc7aCYo0VPemAWFojAM9bOcGxGB4v2ZJ4RkWsD84jjSVgSuc6i+jQHivFH5MaBxLHeE5KccRoy11jcaVjzKhFvD5Qxp3rsIS70j/vzZaSZQqfNddR6PDiI9Zv7Yg+FUhYCMPbVXisfxl+sKlEuaJFPZ2jOWaVccjyi/Ao/4fqFO5fYw7u3fV63N8GzuWxyj+usiLJcdz0XAI58/sa24ZfP7Q/cky4JOL8Ntv0tiJ5Sa9l/n3Inrg/epzTfDw7CXKhTg/NHcQGettT8XBZiLqvxD52JuYykviVMdVyfqJDIaRChvlGxp6L6TrhCTjPOktxfBe+KUAxjW+bzf8Uhnu1KnFpt0dms3N8aDKfMOhyyJ4tdIs1XGIHeCsfD25bn8zMW7F/I84nmYCHmedhR9oMb7ISAj1ueuoFuwC7Prd709wusqBsMv4d+rahv5aZN2E3cb+8A9syeCfCdJO7HzGMHPAD2Nnk/IopK0+s/a6WRC5iE+cEpK1xvpe///+VT7VSMJ6sn6/8W/QN3Zeiv"},
        {"r1","eNrNlT9MU0Ecx+9apC3TMYkSTYmaOAgWN6KGLjgoEXBw0cQOSqILxWjYpIoaYoJIHHQSQiKIDiq6uEh0YBElcdCoaBNj/N93g0UJ7buvv3cP2vfa9zpzQ++un9/3d7/f797dMbamWt3UzeNRH8bZ1qcSyE80+PBTRA0J87a3h3bopvBrwAvXa/rhJf28O1SOq2cI3LgkAn1XYU6WrzAMZEjGBQsclZnTpbGFgeXY6qwFb4Wbi2Hk4sXpgcyFkroA552VOPjNHUE/ZpnDJa+6l3TioMwJ14oi8sg5bcRDXhLx3agrubKE93QVxwG5VJQLHSALPS7yMMb06rVWKCO2ZGi24JJvMu3SrCerjVka7GN78TNeLM6SPRiiv5ryNJiJ16XzvQV9+tlKP8ZYj8lYlUyxDRhYzTj4KqH7kHpu8Shbh9eM7X5T8D9oW4bVIufzSLEai/OThQB32d0O/OXBNPneqb5YKcRKStJBPGDxJixxj++zHuQ/jQTbrLw46Rd5UCIlOjz1rFX+Iy5HRKv05P2GWRtWRpa2y4x58HmoRAT4w+4DnaI8Pjo+KeKkl4gzT34tApVlo4YPlyvcrd9WpnfzY0nu0FN8o3DxjpzOhhuQKS/egx/29pP/sAendKft/JGyuXJyKy70ai6TEaX1qpTnhNYnIkqW+tcc32l9A50r+jKuLlJf5G7/CrgutF1S19+tD9CVhQe0fvvcQnNoYW6aHbky5di+akBimfveqTUYJIuYcJxPVwuPM7rbPvMK13IjJRDzpYIFaYGvFfS8n1IYr2AQohKZiQoG5yyDLsZ9cmBB2hyZ7xbO8+pq26n6wIuJtra+y4eFh0EL9Ouh3k92n/B8QPanYRiffk/dORPzfp623PpoIPNkoNnv/eJV3YNnG8RaenD/A+uclMQ="},
        {"r2","eNpjYBgFAwGElJLxSavtXnscn3zs//8vHQVwSmve+///50Tc2v3+//+/9SpO/Yxx//+9D76hgEuepff/77fKvwNwyavP/f/LiXkKVv2MQBx57/1F7BaDnMSouu/f+2foUgKMgoyMjAyM7uXnzry9//8HI7pWJQUlp+pVs1/f/39q3/9zX5GkmNqY08t77t7MWPXn//+374B+///uCZLJNv/PAYXe/9u99t9/CHj//wdIAurb+/8xAdB9jCoGYPk6LNLvvwHlz00GO2w/Nvn/CxiU6xeD9df/xwauLMjrwiX/7+77/++++/+DmD8fRe7u3WWhzu5AC/76/AfrZ0PS9///MiUloBa1c//+AT29GSTPAzQMAv6UePyEeJsp5z3QBIh+JqU8iOZ1CxiYH0ODXew8UPqVIyRCZcDyv2UDBFgeQcOMMat8zXtYGLOC5T9JL2RgeYQIdpH6iwIo8iyPGXgOIqVJLphhOhDXCa523u+AnAoPQeXjIM5/ee9NC0qKKIbG73qw86c5KymiJhlDqDqw/7+BPI41R3CATX8giCu7yILlL+DMbPHAcP/3H2dmYegH6zfAJc0Edt5f3JkRrP0XIy55HrD8D5zyceBE/wSn8yDJ7yLOwgLk/X/vcetnB5v/Daf9jODsVYC7sBIEgdEagzYAALHxmWQ="},
        {"triangle","eNrNlU1oE0EUx2e2m7ZWhNmgl4rSgwhqA1FBEETjxYsKOXmsBaWg1JrgwVsbT2JBAoIeKtpAQVSQ4AeihzYo+IGaxtCDVUgCPaiIyRzEJukm85xtunFmZzbnPkgC+c37mPefmYfQejQ80BlveEb80D7ne9yO+vBdL7hnAJo+CchoM4xQP/xAhjaD+fEt/38anqCDYc0C3AszfBGwCJqP66oLQQqhELNxDzzVxT9OEwgNwQIZgu86fpeG0eY5SBglWNLFTzcHyCGePgQ0pqmvm9WQlYGqUaK2pfHfxKo4AJDfCPBXl/4wLCAeeniCQgIZKk/TPJlmdZNCg+DLankZmuqi9M82gCWyW90gTx09ARDPAMSCuRWF9wHcKtF6H4O6tZXZES8/TYFbLQk0b+TADnswb5prsUFgy0p32rjalQNIeRqIB9v880UA5j1CZKLNy/yjhOcliTbs1adHwsvY6x8SMYsQpbkiryruphT+laq9FF49PGNSeFV7Nz1zRHis+JtU4Oyo4t7rhnbWVfYqfLub2emi7exOTnFkjT9wm3dA0ockW7ixKkIN4T3PZe3XymOtnzcX2A3JfUyorhVIfGGwrJ1jP6X0SYXPiAL1u3nb1oz5nxzHvgi75zdesbiu9f9tUWzeSZlRYDfFq2O612YeIOvwlUvykzrXwnYa4OWn4u/3HvECk8VV/qGRLV6xCCYe5Yxjj5wCWfp+YVH7KO88X+D8V+N6IaXlxsjt2Url4bvyN5+psOPc1OzXq6Pl134DZcu1qVNn7mSjvgNn/9lg9+S9DiOJoOBItPNQs/A6nLT/ACTwTlk="},
        {"square","eNrNVU1o1EAUTtq1pR4k8YcKIkTQi1Kph/UoVYoXQdpDQUR0EdSCoL3VulYLKh4qrhcvrbAqeHERodhTiwpearUuIoJKs8Ef1Npt5rRLt/l5vkky20x2Eq8dmEmY7/19b968kaS1NmRVScSzs9eT4NavM1MDCRZOEXCXN2txzlMG4CCz+2LwDvDHghhXcwEOI0I8RRj+XQS3FAAF7CLiP0Te8wCmCTNU/5uIu4uAPn2c4os35Qb8qOf580lcHAMy0SwpL/3QzHkCFoFKVH0di919D08ArKj+NqgLODQPXRHz9dyA/YZS7OHxZrKqf54K8fblrR7kC12hR6CJ2N2aH8b1Lc45noDssxvZQemX0fydGucg5Xt+7Nn5AvCpaHcK2DlaC5bIIDjDYGfC+AYfJz3SHn0yB8tFsDmCFwJuuHkY6Y1BRP8Zw1UJE/UbpxP2LxcZfjXTRsqX8a8ahtez3PUaK1qa5g/C5pV8ADubkJp3FNVQgchtTN2ip/yQ5ipcHwqLDirb6XIxol73Dq+oIyet/w0Hr9bVnXbv8+sQl/vWuvqja4L7pez1z4YsLZwI5H6GuW80gt0yS5LdFQ6+A6JjInw5VCMKr8TUNRsPFP7kXG/3KavO1/ylpqHjrOWBuEiC2HzhHgnqfpKUwR2HpVovBzf1v/Azt9vFn4/tpyNlL+/Mesmp5KqY+glJjl5L+Zhn4B48N7B61causmsUI7AOwG00L4ua1iAa+GM4WBQDwqbWl9WJiczAEnfFLefuvzMNE+BDTFdNXxqf1ksNHWFVoG9oVJ8rVeOautLcPVQolO7Gt/2m/WfHpjqTnp10/43Edyd15mDyw9St/edhk9bg+AcQTX/q"},
        {"circle","eNrdlN9LVEEUx2d0NdfWuFusFUTsrkhQEK4RRSgkPdSTbUsu1JMVpkG0EWmSRgX5kA+11AZq2tIPpV7S0giSKF/DEnrq17oQ2EPU3pfUdPfOac7svXPv/mj/AM/DMns/c74z5ztnhpAVGE6lIF7fGimEK4dmlx4UULgEAH8LLH6PcwjnF6CEpvn3/+YXIU415Gbij+ti+1bkizQLV/kRn3oD6fiTnd41sb/P15XQce7y+8hazxmDAtubZ2M2VacJSOYr7YaOU4Ho81ys1ElxYss1h5aocnW/npExKyQxjFFFOmJEKVi4wEVdtyyrh6x8ywUe3TBvWd2CgaW3wn66Jd8EOcEg5c8j/9Uy46zBN8jiku4Zc8JDQz5qqb3R5OM6LzO/UGIH7Vo8FuOKCSN/u1R3EkrbmsnuiHdn8LBX379idAVosfPoDS3Xpt3FwXafnh+3lDUZUBRaAdDeAep4Hg6/XxLkHXP1cn8zmc7cR94C5+IGv5lp3ALnahNb3nNVL78+ky9ifjVTp/QTpmWZ+gtpnqw2/C9Ge5P9g7E4m+Sjj4LDWPcxw180YJ5Qb1XtRj66Iri268sh43wOYttSvc00Xj+D6pT9iTz/CvROKMUZzPH62axL64zIBi0xuI0PniF/7/rRe9vsL+5QShjPy/MT6tBeuY63Wu54SAUNV1vN53FVx/KA90CP2f+VPD+B1TaC6No1L9q815skL0WsKUJHDSN/FFzXGpDdGcWeTokRgxqsZzjo2SHTy4WrqOscVcXLVnEn6DYeUdo5ivb+asb8p/G3+M3R0yKz7Ryq7JPopeJ3Qz4EZS3SWrKZcW9G3MLd8omjIm9Vg1nbZRWWTnBpHNcNfgiLu+u2XvyA/He6//XnmqyXYxvIa0Z6734bGPFlPVvT5mt0pK/z8cnaLIG2YfOcFOJx5jzIHrJy4x/Co0tt"},
        {"cross","eNrNlT0sZFEUx+978/HGR3EJjeoRhWqjkKh2UaBDVFQIiXKnsLuSbYTIhkSoJGgUdJhtNkLCFCJCQUNCZEkwWRvJvMpmGHPP3rn3zoxx7nsKjVPMFL977vmfj3seIe/Qar2x76v3Ad9cvyc3t9c9LwhePna4IMOsGxhcA7akg7SktLP7OwNue9q4410Tq3GH47uETva3rYs05BZr0vDWQ2ASA0P6KKlfcM6GBqflgX9IeNnU741GQhRnQzQPW+uzP5prDVKqBMBTfgTr9MEmBjFC0tuBp94X4mTETzJ6zQXM5IevCov/iOBh4l9p1FXIEjhlcwecYPb6e6LhIkpU8EW35kn18GVic37W1hxoEDgufv9o/A8hZw+2i3plt5gXvJFn72d7EUhg7lPNOx+jUbiliIv043+bK4l5BCc4v8I0b6LEKAkC7GDexnEyfa1hgaMpcjvnN6JQQQZh/DzSzV8UskIspan/Jedy7EIsieX7M+H5/U7C0JYnJhtlwbV+uNRUfoQR3F2ePlOqelJ4AVA+fJlnG0n2Ivcgl3+V4ftY/mcx+NKiv/DSgVx42oJfR48a/NxjwKOvRPuH8XoKcHysxmTtsQ/xCv7k1VIsgmVb11vlTorvRnFxo9nopGjX1oWPZfuA3emHZyvNxO5mJJe7brHyyfpJ3XmAwUGHh3/AfWfI4q7Gw55flPLJV75o1ZS8T/sP5IdnvA=="}
    };
    private final Map<String,Bitmap> cache=new HashMap<>();
    private boolean decoded;

    private void decodeOnce() {
        if(decoded)return;
        decoded=true;
        for(String[] item:DATA) {
            try {
                byte[] compressed=Base64.decode(item[1],Base64.NO_WRAP);
                byte[] packed=new byte[SIZE*SIZE/2];
                int offset=0;
                try(InflaterInputStream zip=new InflaterInputStream(new ByteArrayInputStream(compressed))) {
                    while(offset<packed.length) {
                        int n=zip.read(packed,offset,packed.length-offset);
                        if(n<0)break;
                        if(n==0)continue;
                        offset+=n;
                    }
                    if(offset!=packed.length || zip.read()!=-1)
                        throw new IOException("Corrupt glyph " + item[0]);
                }
                int[] rgba=new int[SIZE*SIZE];
                for(int i=0;i<rgba.length;i++) {
                    int b=packed[i>>1]&255;
                    int alpha=((i&1)==0 ? b>>>4 : b&15)*17;
                    float y=(float)(i/SIZE)/(SIZE-1);
                    // The masks retain their detail, but no longer use GoW's
                    // saturated gold. Uncharted steel/ivory is neutral and
                    // readable against the desaturated slate button fill.
                    int red=Math.round(226-46*y),green=Math.round(234-48*y),
                        blue=Math.round(231-46*y);
                    rgba[i]=(alpha<<24)|(red<<16)|(green<<8)|blue;
                }
                Bitmap bitmap=Bitmap.createBitmap(SIZE,SIZE,Bitmap.Config.ARGB_8888);
                bitmap.setPixels(rgba,0,SIZE,0,0,SIZE,SIZE);
                cache.put(item[0],bitmap);
            } catch(RuntimeException|IOException e) {
                android.util.Log.e("UnchartedTouch","Unable to decode icon " + item[0],e);
            }
        }
    }
    Bitmap get(String name) {decodeOnce();return cache.get(name);}
}
