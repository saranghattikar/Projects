grep "round-trip" out | awk -F" " '{print $4}' | awk -F "/" '{print $2}' >> out_final.text
 
