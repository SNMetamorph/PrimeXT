"use strict";(self.webpackChunkprimext_docs=self.webpackChunkprimext_docs||[]).push([[1270],{3905:(e,t,n)=>{n.d(t,{Zo:()=>d,kt:()=>c});var i=n(7294);function r(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function a(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(e);t&&(i=i.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,i)}return n}function l(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?a(Object(n),!0).forEach((function(t){r(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):a(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function o(e,t){if(null==e)return{};var n,i,r=function(e,t){if(null==e)return{};var n,i,r={},a=Object.keys(e);for(i=0;i<a.length;i++)n=a[i],t.indexOf(n)>=0||(r[n]=e[n]);return r}(e,t);if(Object.getOwnPropertySymbols){var a=Object.getOwnPropertySymbols(e);for(i=0;i<a.length;i++)n=a[i],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(r[n]=e[n])}return r}var s=i.createContext({}),p=function(e){var t=i.useContext(s),n=t;return e&&(n="function"==typeof e?e(t):l(l({},t),e)),n},d=function(e){var t=p(e.components);return i.createElement(s.Provider,{value:t},e.children)},u={inlineCode:"code",wrapper:function(e){var t=e.children;return i.createElement(i.Fragment,{},t)}},m=i.forwardRef((function(e,t){var n=e.components,r=e.mdxType,a=e.originalType,s=e.parentName,d=o(e,["components","mdxType","originalType","parentName"]),m=p(n),c=r,f=m["".concat(s,".").concat(c)]||m[c]||u[c]||a;return n?i.createElement(f,l(l({ref:t},d),{},{components:n})):i.createElement(f,l({ref:t},d))}));function c(e,t){var n=arguments,r=t&&t.mdxType;if("string"==typeof e||r){var a=n.length,l=new Array(a);l[0]=m;var o={};for(var s in t)hasOwnProperty.call(t,s)&&(o[s]=t[s]);o.originalType=e,o.mdxType="string"==typeof e?e:r,l[1]=o;for(var p=2;p<a;p++)l[p]=n[p];return i.createElement.apply(null,l)}return i.createElement.apply(null,n)}m.displayName="MDXCreateElement"},8671:(e,t,n)=>{n.r(t),n.d(t,{assets:()=>s,contentTitle:()=>l,default:()=>u,frontMatter:()=>a,metadata:()=>o,toc:()=>p});var i=n(7462),r=(n(7294),n(3905));const a={sidebar_position:2},l="Installation",o={unversionedId:"eng/installation",id:"eng/installation",title:"Installation",description:"This manual describes the installation of the latest build of PrimeXT, in the future, for release builds, the algorithm will be slightly different.",source:"@site/docs/eng/installation.md",sourceDirName:"eng",slug:"/eng/installation",permalink:"/PrimeXT/docs/eng/installation",draft:!1,editUrl:"https://github.com/SNMetamorph/PrimeXT/tree/master/documentation/docs/eng/installation.md",tags:[],version:"current",sidebarPosition:2,frontMatter:{sidebar_position:2},sidebar:"tutorialSidebar",previous:{title:"Introduction",permalink:"/PrimeXT/docs/eng/intro"},next:{title:"env_dynlight",permalink:"/PrimeXT/docs/eng/entities/env_dynlight"}},s={},p=[{value:"1. Engine installation",id:"1-engine-installation",level:2},{value:"2. PrimeXT development build installation",id:"2-primext-development-build-installation",level:2}],d={toc:p};function u(e){let{components:t,...n}=e;return(0,r.kt)("wrapper",(0,i.Z)({},d,n,{components:t,mdxType:"MDXLayout"}),(0,r.kt)("h1",{id:"installation"},"Installation"),(0,r.kt)("p",null,"This manual describes the installation of the latest build of PrimeXT, in the future, for release builds, the algorithm will be slightly different.\nIf you already have the engine installed, you can skip step 1.\nIt is recommended to periodically manually update the engine, as development does not stand still and bug fixes and new functionality periodically appear."),(0,r.kt)("admonition",{title:"Tip",type:"tip"},(0,r.kt)("p",{parentName:"admonition"},"Keep in mind that PrimeXT only supports latest Xash3D FWGS builds, vanilla Xash3D or old FWGS builds will not work properly.")),(0,r.kt)("h2",{id:"1-engine-installation"},"1. Engine installation"),(0,r.kt)("ul",null,(0,r.kt)("li",{parentName:"ul"},"Select and download ",(0,r.kt)("a",{parentName:"li",href:"https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous"},"Xash3D FWGS engine build")," for your\nplatform (in case of Windows, this is file ",(0,r.kt)("inlineCode",{parentName:"li"},"xash3d-fwgs-win32-i386.7z"),"), then unpack all the files from the archive into a some directory."),(0,r.kt)("li",{parentName:"ul"},"Copy file ",(0,r.kt)("inlineCode",{parentName:"li"},"vgui.dll")," and folder ",(0,r.kt)("inlineCode",{parentName:"li"},"valve")," from your installed ",(0,r.kt)("a",{parentName:"li",href:"https://store.steampowered.com/app/70/HalfLife/"},"Half-Life 1")," to the folder containing the files from the previous step."),(0,r.kt)("li",{parentName:"ul"},"Run ",(0,r.kt)("inlineCode",{parentName:"li"},"xash3d.exe"),"/",(0,r.kt)("inlineCode",{parentName:"li"},"xash3d.sh"),"/",(0,r.kt)("inlineCode",{parentName:"li"},"xash3d")," depending on your platform.")),(0,r.kt)("h2",{id:"2-primext-development-build-installation"},"2. PrimeXT development build installation"),(0,r.kt)("ul",null,(0,r.kt)("li",{parentName:"ul"},"Download ",(0,r.kt)("a",{parentName:"li",href:"https://github.com/SNMetamorph/PrimeXT/releases/tag/continious"},"PrimeXT development build")," for your\nplatform, then unpack all the files from the archive into a engine directory."),(0,r.kt)("li",{parentName:"ul"},"Download ",(0,r.kt)("a",{parentName:"li",href:"https://drive.google.com/file/d/1l3voCVdNi_SlFrOI31ZwABWLQXXUW-Zc/view?usp=sharing"},"PrimeXT content")," and copy folder ",(0,r.kt)("inlineCode",{parentName:"li"},"primext")," from archive into engine directory."),(0,r.kt)("li",{parentName:"ul"},"Installation completed! You can run game using ",(0,r.kt)("inlineCode",{parentName:"li"},"primext.exe")," file")))}u.isMDXComponent=!0}}]);