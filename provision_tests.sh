#!/usr/bin/bash
set -x

PORT="/dev/cu.usbmodem21101"       
#PORT="/dev/cu.usbserial-120" 


#python $HOME/esp/esp-idf/components/esptool_py/esptool/espefuse.py --port $PORT summary --format json --file efuses.json

#python $HOME/esp/esp-idf/components/esptool_py/esptool/espefuse.py --port $PORT summary --format json --file efuses.json

python $HOME/esp/v5.2.1/esp-idf/components/esptool_py/esptool/espefuse.py --port $PORT summary --format json --file efuses.json

#python $HOME/esp/esp-idf/components/esptool_py/esptool/espefuse.py --port $PORT read_mac --format json --file efuses.json
#/Users/Dizr/esp-idf-5.1/esp-idf/components/esptool_py/esptool/espefuse.py
#espefuse.py --port $PORT summary --format json --file efuses.json

# Simplified MAC address extraction
MAC=$(jq -r '.MAC.value' efuses.json | sed 's/ .*//' | tr -d '[:space:]:' | tr -d '\"')
echo $MAC
rm -rf efuses.json



# MAC=$(jq '.MAC.value' efuses.json)
# echo MAC        
# MAC=${MAC%(*}
# echo MAC
# MAC=${MAC//\"/}
# echo MAC
# MAC=${MAC//\ /}
# echo MAC
# MAC=${MAC//:/}
# echo MAC
# rm -rf efuses.json

idf.py clean
idf.py fullclean 
rm -rf sdkconfig
rm -rf build


#dev_id="cld-test-device"
dev_id="cld-$MAC"
IDF_TARGET=esp32c6
cp -rf sdkconfig_v1_wifi_prov sdkconfig

find . -type f -name 'sdkconfig' -exec sed -i '' s/"testClient"/$dev_id/g {} +


thing_type='CloudioV631_type'
thing_group_name='cloudiov631_grp'
product_prj='cloudio'
policy_name='cloudio_v1_policy'
payload_shadow_name='payload-prod'
error_shadow_name='error-logs'
device_info_shadow_name='device-info'

mkdir -p tmp/cert/$dev_id/AWS/
#erase_shadow = '1'

#cd ./fwcerts

thingexists=$(aws iot describe-thing --thing-name "$dev_id" | jq -r '.thingName')
if [ "$thingexists" = "$dev_id" ]; then
    read -p "Thing already exists on AWS press ctrl+c to exit, delete the aws iot thing and run the script again. Press any key to continue... " -n1 -s
    echo''; echo "\nDeleting AWS IoT Thing shadow"
    aws iot-data delete-thing-shadow --thing-name $dev_id --cli-binary-format raw-in-base64-out > /dev/null 2>&1
    #  aws iot-data delete-thing-shadow \
    # --thing-name  $dev_id \
    # /dev/stdout
fi
echo''

echo''; echo "\nDeleting AWS IoT Thing payload shadow"
aws iot-data delete-thing-shadow --thing-name $dev_id --shadow-name $payload_shadow_name --cli-binary-format raw-in-base64-out > /dev/null 2>&1

#aws iot-data delete-thing-shadow --thing-name $dev_id --shadow-name $payload_shadow_name --cli-binary-format raw-in-base64-out --outfile /dev/null

# aws iot-data delete-thing-shadow \
#     --thing-name  $dev_id \
#     --shadow-name $payload_shadow_name

echo''; echo "\nDeleting AWS IoT Thing error shadow"
aws iot-data delete-thing-shadow --thing-name $dev_id --shadow-name $error_shadow_name --cli-binary-format raw-in-base64-out > /dev/null 2>&1

# aws iot-data delete-thing-shadow \
#     --thing-name  $dev_id \
#     --shadow-name $payload_shadow_name

echo "\nCreate AWS IoT Thing"
# Create AWS IoT Thing
aws iot create-thing \
    --thing-name $dev_id \
    --thing-type-name $thing_type

#add to the group
echo''
echo "\nADD Thing to a group"
aws iot add-thing-to-thing-group \
    --thing-name $dev_id \
    --thing-group-name $thing_group_name

# Create keys and certificate
echo''
echo "\nCreate certificates"
arn=$(aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile tmp/cert/$dev_id/AWS/certificate.pem.crt \
    --public-key-outfile tmp/cert/$dev_id/AWS/public.pem.key \
    --private-key-outfile tmp/cert/$dev_id/AWS/private.pem.key |
    jq -r '.certificateArn')

# Attach the right policy
echo''
echo "\nAttach policies"
aws iot attach-policy \
    --target "$arn" \
    --policy-name "$policy_name" 

# Attach ARN
echo''
echo "\nAttach principal"
aws iot attach-thing-principal \
    --principal "$arn" \
    --thing-name "$dev_id"

# reinit payload shadow
echo''
echo "\nUpdate PAYLOAD shadow"
aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $payload_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "desired": null
        }
    }' \
    /dev/stdout

aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $payload_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "reported": null
        }
    }' \
    /dev/stdout
    printf "\n***\nPayload shadow reinitialized for $dev_id \n\n"


# reinit error shadow
echo''
echo "\nUpdate ERROR shadow"
aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $error_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "desired": null
        }
    }' \
    /dev/stdout

aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $error_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "reported": null
        }
    }' \
    /dev/stdout
    printf "\n***\nError shadow reinitialized for $dev_id \n\n"



# reinit device info shadow
echo''
echo "\nUpdate DEVICE INFO shadow"
aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $device_info_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "reported":{
                "app_ver": "vcloudiofw_0_0_0"
                }
        }
    }' \
    /dev/stdout

aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --shadow-name $device_info_shadow_name \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
        "state": {
            "desired":{
                "app_ver": "vcloudiofw_0_0_0"
                }
        }
    }' \
    /dev/stdout

# Update device shadow
echo''
echo "\nUpdate CONFIG Classic Shadow"
aws iot-data update-thing-shadow \
    --thing-name "$dev_id" \
    --cli-binary-format raw-in-base64-out \
    --payload \
    '{
  "state": {
    "desired": {
      "cfg": {
        "publish_interval": 60,
        "facility_id": "default",
        "led": 1,
        "buz": 0,
        "wifi_rst": 0,
        "dbg_mode": 0,
        "sensor_read_interval": 60,
        "radar_read_interval": 30,
        "radar_publish_interval": 60,
        "bdrt": 38400,
        "qry_str": "3101valuer\n\r"
      }
    },
    "reported": {
      "cfg": {
        "publish_interval": 60,
        "facility_id": "default",
        "led": 1,
        "buz": 0,
        "wifi_rst": 0,
        "dbg_mode": 0,
        "sensor_read_interval": 60,
        "radar_read_interval": 30,
        "radar_publish_interval": 60,
        "bdrt": 38400,
        "qry_str": "3101valuer\n\r"
      }
    }
  }
}
' \
    /dev/stdout

    printf "\n***\nThing shadow created for $dev_id with default configuration\n\n"

mkdir -p main/certs/
#cp -rf tmp/cert/$dev_id/AWS/certificate.pem.crt main/certs/client.crt
#cp -rf tmp/cert/$dev_id/AWS/private.pem.key main/certs/client.key

cp -rf tmp/cert/$dev_id/AWS/certificate.pem.crt main/certs/certificate.pem.crt
cp -rf tmp/cert/$dev_id/AWS/private.pem.key main/certs/private.pem.key

idf.py --version 
#idf.py -p $PORT erase-otadata
python $HOME/esp/v5.2.1/esp-idf/components/esptool_py/esptool/esptool.py -p $PORT --chip esp32c6 erase_flash
#python $HOME/esp/esp-idf/components/esptool_py/esptool/esptool.py -p $PORT --chip esp32c6 erase_flash
#parttool.py -p $PORT erase_partition --partition-name=nvs

#idf.py clean
python3 components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p $PORT --keep_ds_data_on_host --ca-cert main/certs/root_cert_auth.crt --device-cert main/certs/certificate.pem.crt --private-key main/certs/private.pem.key --target_chip esp32c6 --secure_cert_type cust_flash

python $HOME/esp/v5.2.1/esp-idf/components/esptool_py/esptool/esptool.py --no-stub --port $PORT write_flash 0xD000 esp_secure_cert_data/esp_secure_cert.bin
idf.py -p $PORT build flash monitor

# # Navigate to the build directory
# cd build

# # Find the file with the highest version number
# latest_file=$(ls vcloudiofw_*.bin | sort -V | tail -n 1)

# # Construct the AWS S3 copy command
# if [ ! -z "$latest_file" ]; then
#     cmd="aws s3 cp $latest_file s3://vaiota-fw-ota-update/$latest_file"
#     echo $cmd
# else
#     echo "No vcloudiofw_*.bin files found!"
# fi
