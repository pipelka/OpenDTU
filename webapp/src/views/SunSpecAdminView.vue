<template>
    <BasePage :title="$t('sunspecadmin.SunSpecSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="alert.show" dismissible :variant="alert.type">
            {{ alert.message }}
        </BootstrapAlert>

        <form @submit="saveSunSpecConfig">
            <CardElement :text="$t('sunspecadmin.Modbus')" textVariant="text-bg-primary">
                <InputElement :label="$t('sunspecadmin.Enabled')"
                    v-model="config.enabled"
                    type="checkbox" wide/>

                <InputElement :label="$t('sunspecadmin.RemoteControl')"
                    v-model="config.remote_control"
                    type="checkbox" wide/>

                <InputElement :label="$t('sunspecadmin.PowerDivider')" v-show="config.remote_control"
                    v-model="config.power_divider"
                    type="number" min="10" max="1000"
                    :tooltip="$t('sunspecadmin.PowerDividerHint')" wide/>

                <InputElement :label="$t('sunspecadmin.Manufacturer')"
                    v-model="config.manufacturer"
                    type="text" maxlength="32" wide/>

                <InputElement :label="$t('sunspecadmin.Model')"
                    v-model="config.model"
                    type="text" maxlength="32" wide/>

                    <CardElement v-for="(inv, index) in config.inverter" :key="`${index}`" :text="inv.name" textVariant="text-bg-secondary">
                    <InputElement :label="$t('sunspecadmin.InverterEnabled')"
                        v-model="inv.enabled"
                        type="checkbox"
                        :tooltip="$t('sunspecadmin.InverterEnabledHint')" wide/>

                    <InputElement :label="$t('sunspecadmin.MaxPower')" v-show="config.remote_control"
                        v-model="inv.max_power"
                        type="number" min="0" max="20000"
                        :tooltip="$t('sunspecadmin.MaxPowerHint')" wide/>

                    <!-- AC Channel Phase Assignment -->
                    <div v-for="(ch, index) in inv.channel_ac" :key="`${index}`">
                        <div class="row">
                            <label :for="`inverter-channel-ac_${index}`" class="col-sm-4 col-form-label">
                                {{ $t('sunspecadmin.ChannelAC', { num: index + 1 }) }}
                            </label>
                            <div class="col-sm-8">
                                <select id="`inverter-channel-ac_${index}`" class="form-select" v-model="ch.phase">
                                    <option v-for="phase in chphaselist" :key="phase.key" :value="phase.key">
                                        {{ $t(`sunspecadmin.PhaseAC`, { num: phase.value }) }}
                                    </option>
                                </select>
                            </div>
                        </div>
                    </div>
                </CardElement>
            </CardElement>
            <FormFooter @reload="getSunSpecConfig"/>
        </form>

    </BasePage>

</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

declare interface ChannelAC {
    phase: number;
}

declare interface Inverter {
    id: string;
    name: string;
    type: string;
    order: number;
    enabled: boolean;
    max_power: number;
    channel_ac: Array<ChannelAC>;
}

declare interface Config {
    enabled: boolean;
    remote_control: boolean;
    manufacturer: string;
    model: string;
    power_divider: number;
    inverter: Array<Inverter>
}

declare interface AlertResponse {
    message: string;
    type: string;
    code: number;
    show: boolean;
}

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement,
    },
    data() {
        return {
            config: {} as Config,
            dataLoading: true,
            alert: {} as AlertResponse,
            chphaselist: [
                { key: 0, value: 1 },
                { key: 1, value: 2 },
                { key: 2, value: 3 },
            ],

        };
    },
    mounted() {
    },
    created() {
        this.getSunSpecConfig();
    },
    methods: {
        getSunSpecConfig() {
            this.dataLoading = true;
            fetch("/api/sunspec/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.config = {
                        enabled: data.enabled,
                        remote_control: data.remote_control,
                        power_divider: data.power_divider,
                        manufacturer: data.manufacturer,
                        model: data.model,
                        inverter: data.inverter.slice().sort((a : Inverter, b: Inverter) => {
                            return a.order - b.order;
                        })
                    };
                    this.dataLoading = false;
                });
        },
        saveSunSpecConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.config));

            fetch("/api/sunspec/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.alert = data;
                    this.alert.message = this.$t('apiresponse.' + data.code, data.param);
                    this.alert.show = true;
                });
        },
    },
});
</script>

<style>
.drag-handle {
    cursor: grab;
}
</style>
