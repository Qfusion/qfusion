#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build-matrix.sh
# Builds the GitHub Actions deployment matrix from server-config.json.
# Called from deploy-servers.yml prepare-matrix job.
#
# Environment variables:
#   CONFIG   - Path to server-config.json (default: .github/server-management/server-config.json)
#   REGIONS  - Comma-separated region keys, or "all"
#   TYPES    - Comma-separated server_type keys, or "all"
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

CONFIG="${CONFIG:-.github/server-management/server-config.json}"
REGIONS="${REGIONS:-all}"
TYPES="${TYPES:-all}"

# Resolve "all" to actual keys from config
if [ "$REGIONS" = "all" ]; then
    REGIONS=$(jq -r '.servers | keys | join(",")' "$CONFIG")
fi
if [ "$TYPES" = "all" ]; then
    TYPES=$(jq -r '.server_types | keys | join(",")' "$CONFIG")
fi

# Split into arrays
IFS=',' read -ra region_list <<< "$REGIONS"
IFS=',' read -ra type_list   <<< "$TYPES"

# Read shared cvar defaults once
defaults=$(jq '.server_defaults.cvars' "$CONFIG")

# Build each matrix entry
entries=()
server_entries=()
for region in "${region_list[@]}"; do
    region="$(echo "$region" | xargs)"
    srv=$(jq --arg r "$region" '.servers[$r] // empty' "$CONFIG")
    [ -z "$srv" ] && continue

    server_entry=$(jq -n \
        --arg region "$region" \
        --argjson srv "$srv" \
        '{
            region:       $region,
            region_label: $srv.label,
            host:         $srv.host,
            key_secret:   $srv.key_secret
        }')
    server_entries+=("$server_entry")

    for stype in "${type_list[@]}"; do
        stype="$(echo "$stype" | xargs)"
        st=$(jq --arg t "$stype" '.server_types[$t] // empty' "$CONFIG")
        [ -z "$st" ] && continue

        # Merge cvars: defaults <- type cvars <- port
        cvars=$(jq -n \
            --argjson defaults "$defaults" \
            --argjson st       "$st" \
            '$defaults + $st.cvars + {net_port: $st.port}')

        # Serialize to "+set key value" pairs
        wf_params=$(jq -r '
            to_entries | map(
                "+set " + .key + " " +
                if (.value | type) == "string" and (.value | test(" "))
                then "\"" + .value + "\""
                else (.value | tostring)
                end
            ) | join(" ")
        ' <<< "$cvars")

        # Assemble the entry
        entry=$(jq -n \
            --arg    region    "$region" \
            --arg    stype     "$stype" \
            --arg    wf_params "+exec configs/${stype}.cfg ${wf_params}" \
            --argjson srv      "$srv" \
            --argjson st       "$st" \
            '{
                region:       $region,
                region_label: $srv.label,
                server_type:  $stype,
                type_label:   $st.label,
                host:         $srv.host,
                key_secret:   $srv.key_secret,
                username:     $srv.username,
                port:         $st.port,
                wf_params:    $wf_params
            }')

        entries+=("$entry")
    done
done

# Validate and emit
if [ "${#entries[@]}" -eq 0 ]; then
    echo "::error::No valid region/type combinations found. Check inputs and server-config.json"
    exit 1
fi

MATRIX=$(printf '%s\n' "${entries[@]}" | jq -s '{include: .}')
SERVER_MATRIX=$(printf '%s\n' "${server_entries[@]}" | jq -s '{include: .}')

echo "matrix=$(jq -c . <<< "$MATRIX")" >> "$GITHUB_OUTPUT"
echo "server_matrix=$(jq -c . <<< "$SERVER_MATRIX")" >> "$GITHUB_OUTPUT"
echo "Generated ${#entries[@]} job(s) across ${#server_entries[@]} server(s):"
jq -r '.include[] | "  \(.region_label) — \(.type_label)"' <<< "$MATRIX"
