import i18n from "@/i18n";

const t = i18n.global.t;

const lang = (code: string) => {
  return t(`err.${code}`);
};

export const showMessage = (status: number | string): string => {
  let message = "";
  switch (status) {
    case 400:
      message = lang("err400");
      break;
    case 401:
      message = lang("err401");
      break;
    case 403:
      message = lang("err403");
      break;
    case 404:
      message = lang("err404");
      break;
    case 408:
      message = lang("err408");
      break;
    case 500:
      message = lang("err500");
      break;
    case 501:
      message = lang("err501");
      break;
    case 502:
      message = lang("err502");
      break;
    case 503:
      message = lang("err503");
      break;
    case 504:
      message = lang("err504");
      break;
    case 505:
      message = lang("err505");
      break;
    default:
      message = lang("errxxx");
  }
  return `${message}, ${lang("errhint0")}`;
};
